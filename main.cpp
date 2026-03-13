#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <format>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

using namespace glm;

constexpr u32 MAP_WIDTH = 9;
constexpr f32 MOVE_TIME = 0.2f;
constexpr f32 WATER_MOVE_TIME = 0.1f;
constexpr u32 MAX_LEVEL = 8;
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
std::array<SDL_Texture*, 5> flameTiles;
SDL_Texture* flameTile;
SDL_Texture* waterSprite;
SDL_Texture* playerSprite;
SDL_Texture* fontAtlas;

i32 windowWidth;
i32 windowHeight;

ivec2 newPlayerPos;
ivec2 playerPos;
f32 moveTimer;

ivec2 waterTarget;
f32 waterTime;
f32 waterAnimTimer;
bool water;

u32 startingTrees;
u32 lostTrees;
u32 maxLostTrees;

bool gameWon;
u32 currentLevel;

struct MapCell
{
    bool hasTree = false;
    u32 fireCount = 0;
};

std::array<MapCell, MAP_WIDTH * MAP_WIDTH> map{};

inline MapCell& CellAt(ivec2 pos)
{
    return map[pos.x + pos.y * MAP_WIDTH];
}

bool LoadLevel(u32 levelNum)
{
    if (levelNum > MAX_LEVEL)
    {
        levelNum = 1;
    }

    std::string filename = std::format("../maps/level{}.txt", levelNum);

    std::ifstream mapFile{filename};

    moveTimer = 0;
    water = false;
    gameWon = false;

    std::string line;
    if (!std::getline(mapFile, line))
    {
        return false;
    }
    maxLostTrees = std::stoi(line);
    lostTrees = 0;
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
                CellAt({col, row}) = {true};
                startingTrees++;
                break;
            case 'P':
                playerPos = {col, row};
                newPlayerPos = playerPos;
                CellAt({col, row}) = {};
                break;
            case '1':
                CellAt({col, row}) = {true, 1};
                startingTrees++;
                break;
            case '2':
                CellAt({col, row}) = {true, 2};
                startingTrees++;
                break;
            case '3':
                CellAt({col, row}) = {true, 3};
                startingTrees++;
                break;
            case '4':
                CellAt({col, row}) = {true, 4};
                startingTrees++;
                break;
            case '5':
                CellAt({col, row}) = {true, 5};
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
    std::string path = "../textures/" + filename + ".png";
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
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
                lostTrees++;
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
    ivec2 pos = playerPos;
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
    SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE);

    groundTile = LoadTexture("ground_tile");
    treeTile = LoadTexture("tree_base");
    flameTiles[0] = LoadTexture("flame_1");
    flameTiles[1] = LoadTexture("flame_2");
    flameTiles[2] = LoadTexture("flame_3");
    flameTiles[3] = LoadTexture("flame_4");
    flameTiles[4] = LoadTexture("flame_5");
    waterSprite = LoadTexture("water");
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
                    playerPos = newPlayerPos;
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
                    if (newPlayerPos != playerPos)
                    {
                        if (CellAt(newPlayerPos).hasTree)
                        {
                            newPlayerPos = playerPos;
                        }
                        else
                        {
                            moveTimer = 1.0;
                            AdvanceTime();
                        }
                    }
                    if (event.key.key == SDLK_UP)
                    {
                        ShootWater({0, -1});
                    }
                    if (event.key.key == SDLK_DOWN)
                    {
                        ShootWater({0, 1});
                    }
                    if (event.key.key == SDLK_LEFT)
                    {
                        ShootWater({-1, 0});
                    }
                    if (event.key.key == SDLK_RIGHT)
                    {
                        ShootWater({1, 0});
                    }
                    if (event.key.key == SDLK_SPACE)
                    {
                        AdvanceTime();
                    }
                    if (event.key.key == SDLK_R)
                    {
                        LoadLevel(currentLevel);
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0x60, 0xa0, 0x40, 0xff);
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
                if (PosInBounds(waterTarget))
                {
                    CellAt(waterTarget).fireCount = 0;
                    CheckWin();
                }
            }
        }

        if (water)
        {
            vec2 waterPosInterp = static_cast<vec2>(waterTarget) +
                (static_cast<vec2>(playerPos) - static_cast<vec2>(waterTarget)) * waterAnimTimer;
            SDL_FRect waterRect = {waterPosInterp.x, waterPosInterp.y, 1, 1};
            SDL_RenderTexture(renderer, waterSprite, nullptr, &waterRect);
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
            }
        }

        vec2 playerPosInterp = static_cast<vec2>(newPlayerPos) +
            (static_cast<vec2>(playerPos) - static_cast<vec2>(newPlayerPos)) * moveTimer;
        SDL_FRect playerRect = {playerPosInterp.x, playerPosInterp.y, 1, 1};
        SDL_RenderTexture(renderer, playerSprite, nullptr, &playerRect);

        std::string status = std::format("Level {}\nTrees Alive: {}\nMinimum Trees: {}", currentLevel, startingTrees - lostTrees, startingTrees - maxLostTrees);
        DrawText(status, {0.25, MAP_WIDTH + 0.125});

        SDL_RenderPresent(renderer);

        if (lostTrees > maxLostTrees)
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