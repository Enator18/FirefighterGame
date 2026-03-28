#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <format>
#include <vector>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

using namespace glm;

constexpr u32 MAP_WIDTH = 9;
constexpr f32 MOVE_TIME = 0.2f;
constexpr f32 WATER_MOVE_TIME = 0.1f;
constexpr u32 MAX_LEVEL = 9;
constexpr u32 FONT_HEIGHT = 10;
constexpr std::array<u32, 96> FONT_WIDTHS
{
    4, 4, 5, 6, 7, 6, 6, 3, 4, 5, 5, 6, 3, 6, 2, 6,
    6, 5, 6, 6, 6, 6, 6, 6, 6, 6, 2, 2, 5, 6, 5, 6,
    7, 6, 6, 6, 6, 6, 6, 6, 6, 4, 7, 7, 6, 8, 7, 7,
    6, 7, 7, 6, 6, 7, 6, 8, 6, 6, 6, 4, 6, 4, 6, 6,
    3, 6, 6, 6, 6, 6, 5, 6, 6, 2, 6, 5, 4, 8, 6, 6,
    6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 7, 5, 2, 5, 7, 6
};

SDL_Window* window;
SDL_Renderer* renderer;

SDL_Texture* groundTile;
SDL_Texture* treeTile;
SDL_Texture* treeTrunkTile;
std::array<SDL_Texture*, 5> flameTiles;
SDL_Texture* flameTile;
SDL_Texture* waterSprite;
SDL_Texture* waterTankTile;
SDL_Texture* bigWaterSprite;
SDL_Texture* playerSprite;
SDL_Texture* fontAtlas;

i32 windowWidth;
i32 windowHeight;

ivec2 newPlayerPos;
f32 moveTimer;

ivec2 waterTarget;
f32 waterTime;
f32 waterAnimTimer;
bool water;

u32 startingTrees;
u32 maxLostTrees;

bool gameWon;
u32 currentLevel;

struct MapCell
{
    u32 fireCount = 0;
    bool hasTree = false;
    bool hasTank = false;
};

struct GameState
{
    std::array<MapCell, MAP_WIDTH * MAP_WIDTH> map{};
    ivec2 playerPos;
    u32 lostTrees;
    bool bigWater = false;
};

GameState state;

std::vector<GameState> previousStates;

inline MapCell& CellAt(ivec2 pos)
{
    return state.map[pos.x + pos.y * MAP_WIDTH];
}

bool LoadLevel(u32 levelNum)
{
    if (levelNum > MAX_LEVEL)
    {
        levelNum = 1;
    }

    std::string filename = std::format("maps/level{}.txt", levelNum);

    std::ifstream mapFile{filename};

    previousStates.clear();

    moveTimer = 0;
    water = false;
    gameWon = false;

    std::string line;
    if (!std::getline(mapFile, line))
    {
        return false;
    }
    maxLostTrees = std::stoi(line);
    state.lostTrees = 0;
    state.bigWater = false;
    startingTrees = 0;

    for (u32 row = 0; row < MAP_WIDTH; row++)
    {
        if (!std::getline(mapFile, line))
        {
            return false;
        }
        if (line.length() < MAP_WIDTH)
        {
            return false;
        }
        for (u32 col = 0; col < MAP_WIDTH; col++)
        {
            switch (line[col])
            {
            case 'T':
                CellAt({col, row}) = {0, true};
                startingTrees++;
                break;
            case 'P':
                state.playerPos = {col, row};
                newPlayerPos = state.playerPos;
                CellAt({col, row}) = {};
                break;
            case 'W':
                CellAt({col, row}) = {0, false, true};
                break;
            case '1':
                CellAt({col, row}) = {1, true};
                startingTrees++;
                break;
            case '2':
                CellAt({col, row}) = {2, true};
                startingTrees++;
                break;
            case '3':
                CellAt({col, row}) = {3, true};
                startingTrees++;
                break;
            case '4':
                CellAt({col, row}) = {4, true};
                startingTrees++;
                break;
            case '5':
                CellAt({col, row}) = {5, true};
                startingTrees++;
                break;
            default:
                CellAt({col, row}) = {};
                break;
            }
        }
    }

    currentLevel = levelNum;
    return true;
}

inline bool PosInBounds(i32 pos)
{
    return pos >= 0 && pos < MAP_WIDTH;
}

inline bool PosInBounds(ivec2 pos)
{
    return PosInBounds(pos.x) && PosInBounds(pos.y);
}

SDL_Texture* LoadTexture(std::string filename)
{
    std::string path = "textures/" + filename + ".png";
    SDL_Surface* surface = SDL_LoadPNG(path.c_str());
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr)
    {
        std::cerr << "Failed to create texture from " << filename << "! SDL_Error: " << SDL_GetError() << std::endl;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
    return texture;
}

void IgniteNeighbors(i32 row, i32 col)
{
    for (i32 rowOff = -1; rowOff <= 1; rowOff++)
    {
        for (i32 colOff = -1; colOff <= 1; colOff++)
        {
            i32 neighborRow = row + rowOff;
            i32 neighborCol = col + colOff;
            if (PosInBounds(neighborRow) && PosInBounds(neighborCol))
            {
                MapCell& cell = CellAt({neighborCol, neighborRow});
                if (cell.hasTree && cell.fireCount < 5)
                {
                    cell.fireCount++;
                }
            }
        }
    }
}

void CheckWin()
{
    u32 totalFire = 0;
    for (u32 row = 0; row < MAP_WIDTH; row++)
    {
        for (u32 col = 0; col < MAP_WIDTH; col++)
        {
            if (CellAt({col, row}).fireCount > 0)
            {
                totalFire++;
            }
        }
    }

    if (totalFire == 0)
    {
        gameWon = true;
    }
}

void AdvanceTime()
{
    previousStates.push_back(state);
    for (u32 row = 0; row < MAP_WIDTH; row++)
    {
        for (u32 col = 0; col < MAP_WIDTH; col++)
        {
            MapCell& cell = CellAt({col, row});
            if (cell.fireCount > 0)
            {
                cell.fireCount++;
            }
        }
    }
    for (u32 row = 0; row < MAP_WIDTH; row++)
    {
        for (u32 col = 0; col < MAP_WIDTH; col++)
        {
            MapCell& cell = CellAt({col, row});
            if (cell.fireCount > 5)
            {
                cell.fireCount = 0;
                cell.hasTree = false;
                IgniteNeighbors(row, col);
                state.lostTrees++;
            }
        }
    }
    CheckWin();
}

void ShootWater(ivec2 direction)
{
    AdvanceTime();
    water = true;
    waterAnimTimer = 1.0;
    waterTime = 0;
    ivec2 pos = state.playerPos;
    while (PosInBounds(pos) && !CellAt(pos).hasTree)
    {
        pos += direction;
        waterTime += WATER_MOVE_TIME;
    }
    waterTarget = pos;
}

void DrawText(std::string& text, vec2 pos)
{
    vec2 currentPos = pos;
    for (u8 c : text)
    {
        if (c == '\n')
        {
            currentPos.x = pos.x;
            currentPos.y += FONT_HEIGHT / 16.0;
        }
        else
        {
            u32 charRow = c / 16;
            u32 charCol = c % 16;

            f32 width = 16;
            if (c > 31 && c < 128)
            {
                width = FONT_WIDTHS[c - 32];
            }

            SDL_FRect srcRect = {charCol * 8.0f, charRow * 8.0f, width, 8};
            SDL_FRect destRect = {currentPos.x, currentPos.y, width / 16.0f, 0.5};
            SDL_RenderTexture(renderer, fontAtlas, &srcRect, &destRect);

            currentPos.x += width / 16.0f;
        }
    }
}

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    window = SDL_CreateWindow("Firefighter Game", 900, 1100, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_SetRenderLogicalPresentation(renderer, MAP_WIDTH, MAP_WIDTH + 2, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    groundTile = LoadTexture("ground_tile");
    treeTile = LoadTexture("tree_base");
    flameTiles[0] = LoadTexture("flame_1");
    flameTiles[1] = LoadTexture("flame_2");
    flameTiles[2] = LoadTexture("flame_3");
    flameTiles[3] = LoadTexture("flame_4");
    flameTiles[4] = LoadTexture("flame_5");
    waterSprite = LoadTexture("water");
    waterTankTile = LoadTexture("water_tank");
    bigWaterSprite = LoadTexture("big_water");
    playerSprite = LoadTexture("player");
    fontAtlas = LoadTexture("font_atlas");

    if (!LoadLevel(1))
    {
        return 1;
    }

    u64 then = SDL_GetPerformanceCounter();

    while (true)
    {
        u64 now = SDL_GetPerformanceCounter();
        f32 delta = ((f32)(now - then)) / SDL_GetPerformanceFrequency();
        then = now;

        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                return 0;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                {
                    return 0;
                }
                if (moveTimer <= 0 && !water)
                {
                    state.playerPos = newPlayerPos;
                    if (event.key.key == SDLK_W && newPlayerPos.y > 0)
                    {
                        newPlayerPos.y--;
                    }
                    if (event.key.key == SDLK_S && newPlayerPos.y < MAP_WIDTH - 1)
                    {
                        newPlayerPos.y++;
                    }
                    if (event.key.key == SDLK_A && newPlayerPos.x > 0)
                    {
                        newPlayerPos.x--;
                    }
                    if (event.key.key == SDLK_D && newPlayerPos.x < MAP_WIDTH - 1)
                    {
                        newPlayerPos.x++;
                    }
                    if (newPlayerPos != state.playerPos)
                    {
                        if (CellAt(newPlayerPos).hasTree)
                        {
                            newPlayerPos = state.playerPos;
                        }
                        else
                        {
                            moveTimer = 1.0;
                            MapCell& newCell = CellAt(newPlayerPos);
                            if (newCell.hasTank)
                            {
                                newCell.hasTank = false;
                                state.bigWater = true;
                            }
                            AdvanceTime();
                        }
                    }
                    switch (event.key.key)
                    {
                        case SDLK_UP:
                            ShootWater({0, -1});
                            break;
                        case SDLK_DOWN:
                            ShootWater({0, 1});
                            break;
                        case SDLK_LEFT:
                            ShootWater({-1, 0});
                            break;
                        case SDLK_RIGHT:
                            ShootWater({1, 0});
                            break;
                        case SDLK_SPACE:
                            AdvanceTime();
                            break;
                        case SDLK_R:
                            LoadLevel(currentLevel);
                            break;
                        case SDLK_Z:
                            if (!previousStates.empty())
                            {
                                state = previousStates.back();
                                previousStates.pop_back();
                                newPlayerPos = state.playerPos;
                            }
                            break;
                    }
                }
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0xcf, 0x19, 0x19, 0xff);
        SDL_RenderClear(renderer);

        SDL_FRect background = {0, 0, 9, 9};
        SDL_RenderTextureTiled(renderer, groundTile, nullptr, 1.0 / groundTile->w, &background);

        if (moveTimer > 0)
        {
            moveTimer -= delta / MOVE_TIME;
        }
        if (moveTimer < 0)
        {
            moveTimer = 0;
        }

        if (water)
        {
            waterAnimTimer -= delta / waterTime;

            if (waterAnimTimer < 0)
            {
                water = false;
                if (state.bigWater)
                {
                    for (i32 x = -1; x < 2; x++)
                    {
                        for (i32 y = -1; y < 2; y++)
                        {
                            ivec2 treePos = waterTarget + ivec2{x, y};
                            if (PosInBounds(treePos))
                            {
                                CellAt(treePos).fireCount = 0;
                            }
                            CheckWin();
                        }
                    }
                }
                else
                {
                    if (PosInBounds(waterTarget))
                    {
                        CellAt(waterTarget).fireCount = 0;
                        CheckWin();
                    }
                }
                state.bigWater = false;
            }

            vec2 waterPosInterp = static_cast<vec2>(waterTarget) +
                            (static_cast<vec2>(state.playerPos) - static_cast<vec2>(waterTarget)) * waterAnimTimer;
            SDL_FRect waterRect = {waterPosInterp.x, waterPosInterp.y, 1, 1};
            if (state.bigWater)
            {
                SDL_RenderTexture(renderer, bigWaterSprite, nullptr, &waterRect);
            }
            else
            {
                SDL_RenderTexture(renderer, waterSprite, nullptr, &waterRect);
            }
        }

        for (f32 row = 0; row < MAP_WIDTH; row++)
        {
            for (f32 col = 0; col < MAP_WIDTH; col++)
            {
                SDL_FRect tileRect = {col, row, 1, 1};

                MapCell& cell = CellAt({col, row});
                if (cell.hasTree)
                {
                    SDL_RenderTexture(renderer, treeTile, nullptr, &tileRect);

                    if (cell.fireCount > 0)
                    {
                        SDL_RenderTexture(renderer, flameTiles[cell.fireCount - 1], nullptr, &tileRect);
                    }
                }
                if (cell.hasTank)
                {
                    SDL_RenderTexture(renderer, waterTankTile, nullptr, &tileRect);
                }
            }
        }

        vec2 playerPosInterp = static_cast<vec2>(newPlayerPos) +
            (static_cast<vec2>(state.playerPos) - static_cast<vec2>(newPlayerPos)) * moveTimer;
        SDL_FRect playerRect = {playerPosInterp.x, playerPosInterp.y, 1, 1};
        SDL_RenderTexture(renderer, playerSprite, nullptr, &playerRect);

        std::string status = std::format("Level {}\nTrees Alive: {}\nMinimum Trees: {}",
            currentLevel, startingTrees - state.lostTrees, startingTrees - maxLostTrees);
        DrawText(status, {0.25, MAP_WIDTH + 0.125});

        SDL_RenderPresent(renderer);

        if (state.lostTrees > maxLostTrees)
        {
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_INFORMATION,
                "You lost...",
                "Too many trees burned down...",
                window);
            LoadLevel(currentLevel);
        }
        else if (gameWon)
        {
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_INFORMATION,
                "You won!",
                "You saved the forest.",
                window);
            LoadLevel(currentLevel + 1);
        }
    }
}