#include <genesis.h>
#include "resources.h"
#include "player.h"
#include "soldier.h"
#include "lvl1.h"
// Constantes
#define MAX_BULLETS 5
#define MAX_ENEMIES 10
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 180
#define GROUND_Y 180
#define PLAYER_SPEED 2
#define PLAYER_JUMP_POWER -12
#define GRAVITY 1
#define BULLET_SPEED 4
#define BULLET_MAX_DISTANCE 180
#define ENEMY_SPEED 1
#define ENEMY_ATTACK_RANGE 30
#define ENEMY_TYPE_SOLDIER 0
#define ENEMY_TYPE_RANGE   1
#define ENEMY_TYPE_TANK    2
#define ENEMY_TYPE_FLYING  3
#define SHOOT_COOLDOWN 15
#define PLAYER_MAX_HP 100
#define ENEMY_MAX_HP 50
#define BULLET_DAMAGE 25
#define ENEMY_DAMAGE 10
// Nombre de frames entre chaque ajustement vertical du BG_A (plus grand = plus lent)
#define BGA_SCROLL_FRAME_DELAY 8

// Structure pour le joueur
typedef struct {
    Sprite* sprite;
    s16 x;
    s16 y;
    s16 groundY;
    s16 velX;
    s16 velY;
    u8 facingRight;
    u8 shootCooldown;
    u8 shootAnimTimer;
    u8 onGround;
    s16 hp;
    s16 maxHp;
    u8 invincible;
    u8 invincibleTimer;
    u8 blockHorizDuringJump; // flag: when jumping, block horizontal input if true
} Player;

// Structure pour les projectiles
typedef struct {
    Sprite* sprite;
    s16 x;
    s16 y;
    s16 startX;
    u8 active;
    u8 exploding;
    u8 explodeTimer;
    u8 facingRight;
} Bullet;

// Structure pour les ennemis
typedef struct {
    Sprite* sprite;
    u8 type;
    s16 x;
    s16 y;
    s16 velY;
    u8 onGround;
    u8 active;
    u8 facingRight;
    u8 attacking;
    u8 attackCooldown;
    u8 dying;
    u8 deathTimer;
    // Hitbox
    s16 hitboxOffsetX;
    s16 hitboxOffsetY;
    s16 hitboxWidth;
    s16 hitboxHeight;
    s16 hp;
    s16 maxHp;
    u8 speed;
    s16 attackRange;
    u8 damage;
    // AI helpers
    s16 baseY;     // spawn Y for flying hover center
    s16 aiTimer;   // generic AI timer / phase
    s16 baseX;     // spawn X for flying hover center
    u8 diveState;  // 0=idle,1=diving,2=returning
    u8 diveTimer;  // frames remaining for dive
} Enemy;

// Structure pour un niveau
typedef struct {
    const MapDefinition* bga;   // Background A
    const MapDefinition* bgb;   // Background B
    const TileSet* bgaTileset;  // Tileset BG A
    const TileSet* bgbTileset;  // Tileset BG B
    const Palette* bgaPalette;  // Palette BG A
    const Palette* bgbPalette;  // Palette BG B
    const SpriteDefinition* enemySprite;  // Sprite des ennemis
    const Palette* enemyPalette;  // Palette des ennemis
    const u16* collisionMap;    // Map de collision du niveau
    u16 mapWidth;               // Largeur de la map en tuiles
    u16 mapHeight;              // Hauteur de la map en tuiles
    u8 enemyCount;            // Nombre d'ennemis
    s16 enemySpawnX[MAX_ENEMIES];  // Positions X de spawn
    s16 enemySpawnY[MAX_ENEMIES];  // Positions Y de spawn
    u8 enemySpawnType[MAX_ENEMIES]; // Type d'ennemis à spawn
    s16 bgaOffsetY; // Offset vertical BG_A pour ce niveau
    s16 bgbOffsetY; // Offset vertical BG_B pour ce niveau
    s16 bgaMaxOffsetY; // Offset vertical maximum autorisé pour BG_A
    u8 bgaScrollDelay; // Nombre de frames entre chaque ajustement vertical du BG_A
    u8 bgbScrollDelay; // Nombre de frames entre chaque ajustement vertical du BG_B
    u8 bgaScrollStep;  // Pixels à ajuster par pas pour BG_A (0 = disabled)
    u8 bgbScrollStep;  // Pixels à ajuster par pas pour BG_B (0 = disabled)
} Level;

// Variables globales
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
s16 cameraX = 0;
u16 mapWidth = 0; // Will be initialized in loadLevel()
s16 mapHeight = 0; // Hauteur de la map en pixels, initialisée dans loadLevel()
s16 lastHitEnemyIndex = -1;
u8 lastHitEnemyDisplayTimer = 0;

// Variables pour éviter le clignotement du HUD
s16 lastPlayerHP = -1;
u8 lastEnemyCount = 255;
s16 lastDisplayedEnemyHP = -1;
u16 VDPTilesFilled = TILE_USER_INDEX;

// Variables pour les maps de background
Map* bgMap = NULL;
Map* bgbMap = NULL;

// Ajout des offsets verticaux pour les backgrounds
u16 cameraY = 0;      // Offset vertical principal (BG_A)
u16 bgbCameraY = 0;   // Offset vertical pour BG_B

// Offsets manuels pour BG_A et BG_B
// Les offsets sont maintenant par niveau (dans Level)

// Gestion des niveaux
u8 currentLevel = 0;
Level levels[3];  // 3 niveaux pour l'exemple

// Track loaded tilesets and sprite gfx to avoid duplicate VDP loads
#define MAX_LOADED_TILESETS 16
typedef struct { const TileSet* ts; u16 index; } LoadedTileSet;
static LoadedTileSet loadedTileSets[MAX_LOADED_TILESETS];
static u8 loadedTileSetCount = 0;

#define MAX_LOADED_GFX 16
typedef struct { const void* gfx; u16 index; } LoadedGfx;
static LoadedGfx loadedGfx[MAX_LOADED_GFX];
static u8 loadedGfxCount = 0;

// Prototypes
void initLevels();
u16 loadLevel(u8 levelIndex,u16 index);
void initGame();
// VRAM helpers
u16 ensureTilesetLoaded(const TileSet* ts);
u16 ensureGfxLoaded(const SpriteDefinition* spr);
void updatePlayer();
void updateBullets();
void updateEnemies();
void updateCamera();
void shootBullet();
u8 canMoveToPosition(s16 x, s16 y);
void checkAdjacentSolids(s16 px, s16 py, u8* leftSolid, u8* rightSolid);
void handleInput();
void checkCollisions();
void spawnEnemy(s16 x, s16 y, u8 type);
void drawHUD();

int main() {

    VDP_setWindowVPos(TRUE, 21);
    VDP_setTextPlane(WINDOW);
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    SPR_init();
    
    // Charger les palettes
    PAL_setPalette(PAL1, palette_player.data, CPU);
    PAL_setPalette(PAL2, palette_soldier.data, CPU);
    
    // Définir la couleur de fond (backdrop) en bleu
    VDP_setBackgroundColor(16); // Index 16 de la palette (bleu dans la palette par défaut)
    
    initLevels();

    loadLevel(currentLevel,VDPTilesFilled);  // Charger le niveau 2
    initGame();
    

    // Boucle principale
    while(1) {
        handleInput();
        updatePlayer();
        updateBullets();
        updateEnemies();
        updateCamera();
        checkCollisions();
        drawHUD();
        SPR_update();
        SYS_doVBlankProcess();
    }
    
    return 0;
}

void initLevels() {
    // Niveau 1
    levels[0].bga = &bga_map;
    levels[0].bgb = &bgb_map;
    levels[0].bgaTileset = &bga_tileset;
    levels[0].bgbTileset = &bgb_tileset;
    levels[0].bgaPalette = &palette_lvl;
    levels[0].bgbPalette = &palette_lvlbg;
    levels[0].enemySprite = &sprite_soldier;
    levels[0].enemyPalette = &palette_soldier;
    levels[0].collisionMap = levelMap;  // Map de collision du niveau 1
    levels[0].mapWidth = 191;
    levels[0].mapHeight = 20;
    levels[0].bgaOffsetY = 0;
    levels[0].bgbOffsetY = 0;
    levels[0].bgaMaxOffsetY = 0;
    levels[0].enemyCount = 8;
    levels[0].bgaScrollDelay = 8;
    levels[0].bgbScrollDelay = 8;
    levels[0].enemySpawnX[0] = 300;
    levels[0].enemySpawnType[0] = 0; // soldier
    levels[0].bgaScrollStep = 0;
    levels[0].bgbScrollStep = 0;
    levels[0].enemySpawnY[0] = 0;
    levels[0].enemySpawnX[1] = 450;
    levels[0].enemySpawnType[1] = 1; // ranged
    levels[0].enemySpawnY[1] = 0;
    levels[0].enemySpawnX[2] = 600;
    levels[0].enemySpawnType[2] = 0;
    levels[0].enemySpawnY[2] = 0;
    levels[0].enemySpawnX[3] = 800;
    levels[0].enemySpawnType[3] = 2; // tank
    levels[0].enemySpawnY[3] = 0;
    levels[0].enemySpawnX[4] = 950;
    levels[0].enemySpawnType[4] = 0;
    levels[0].enemySpawnY[4] = 0;
    levels[0].enemySpawnX[5] = 1100;
    levels[0].enemySpawnType[5] = 0;
    levels[0].enemySpawnY[5] = 0;
    levels[0].bgaMaxOffsetY = 256; // Allow more vertical scroll for BG_A
    levels[0].enemySpawnY[6] = 0;
    levels[0].enemySpawnType[6] = 1;
    levels[0].enemySpawnX[7] = 1000;
    levels[0].enemySpawnType[7] = 3; // flying
    levels[0].enemySpawnY[7] = 0;
    
    // Niveau 2 (exemple - même configuration pour l'instant)
    levels[1].bga = &lvl2bga_map;
    levels[1].bgb = &lvl2bgb_map;
    levels[1].bgaTileset = &lvl2bga_tileset;
    levels[1].bgbTileset = &lvl2bgb_tileset;
    levels[1].bgaPalette = &lvl2bga_pal;
    levels[1].bgbPalette = &lvl2bgb_pal ;
    levels[1].bgaMaxOffsetY = 0; // Allow more vertical scroll for BG_A
    levels[1].enemySprite = &sprite_range1;
    levels[1].enemyPalette = &palette_range1;
    levels[1].collisionMap = levelMap2;  // Map de collision du niveau 3
    levels[1].mapWidth = LVL2_MAP_WIDTH;
    levels[1].mapHeight = LVL2_MAP_HEIGHT;
    levels[1].enemyCount = 8;
    levels[1].bgaMaxOffsetY = 0;
    levels[1].bgaScrollDelay = 0;
    levels[1].bgbScrollDelay = 0;
    levels[1].bgaScrollStep = 0;
    levels[1].bgbScrollStep = 0;
    for(u8 i = 0; i < 8; i++) {
        levels[1].enemySpawnX[i] = levels[0].enemySpawnX[i];
        levels[1].enemySpawnY[i] = levels[0].enemySpawnY[i];
        levels[1].enemySpawnType[i] = levels[0].enemySpawnType[i];
    }
    // Niveau 3 (exemple - même configuration pour l'instant)
    levels[2].bga = &lvl3bga_map;
    levels[2].bgb = &lvl3bgb_map;
    levels[2].bgaTileset = &lvl3bga_tileset;
    levels[2].bgbTileset = &lvl3bgb_tileset;
    levels[2].bgaPalette = &lvl3bga_pal;
    levels[2].bgbPalette = &lvl3bgb_pal ;
    levels[2].enemySprite = &sprite_soldier;
    levels[2].enemyPalette = &palette_soldier;
    levels[2].collisionMap = levelMap3;  // Map de collision du niveau 2
    levels[2].mapWidth = LVL3_MAP_WIDTH;
    levels[2].mapHeight = LVL3_MAP_HEIGHT;
    levels[2].bgaOffsetY = 135;
    levels[2].bgbOffsetY = 95;
    levels[2].bgaMaxOffsetY = 1000;
    levels[2].enemyCount = 8;
    levels[2].bgaScrollDelay = 3;
    levels[2].bgbScrollDelay =25;
    levels[2].bgaScrollStep = 1;
    levels[2].bgbScrollStep = 2;
    for(u8 i = 0; i < 8; i++) {
        levels[2].enemySpawnX[i] = levels[0].enemySpawnX[i];
        levels[2].enemySpawnY[i] = levels[0].enemySpawnY[i];
        levels[2].enemySpawnType[i] = levels[0].enemySpawnType[i];
    }
}

u16 loadLevel(u8 levelIndex,u16 index) {
    if(levelIndex >= 3) levelIndex=0;  // Sécurité
    
    currentLevel = levelIndex;
    Level* level = &levels[levelIndex];
    
    // Libérer les anciennes maps si elles existent
    if(bgMap != NULL) {
        MAP_release(bgMap);
        bgMap = NULL;
    }
    if(bgbMap != NULL) {
        MAP_release(bgbMap);
        bgbMap = NULL;
    }

    // Clear VRAM tilemap planes to avoid leftover tiles/indexes
    // Clear full BG_A and BG_B tilemaps (covering typical 64x32 tilemap)
    VDP_clearTileMapRect(BG_A, 0, 0, 64, 32);
    VDP_clearTileMapRect(BG_B, 0, 0, 64, 32);
    cameraX=0;
    cameraY = 0;
    bgbCameraY = 0;
    // Reset our loaded tiles/gfx trackers so subsequent loads start fresh
    loadedTileSetCount = 0;
    loadedGfxCount = 0;
    VDPTilesFilled = TILE_USER_INDEX;
    
    // Charger les palettes (si fournies)
    if(level->bgaPalette) {
        PAL_setPalette(PAL0, level->bgaPalette->data, CPU);
    }
    if(level->bgbPalette) {
        PAL_setPalette(PAL3, level->bgbPalette->data, CPU);
    }
    if(level->enemyPalette) {
        PAL_setPalette(PAL2, level->enemyPalette->data, CPU);
    }

    // Ensure tilesets are loaded once and get their VRAM indices (guarded)
    if(level->bgaTileset && level->bga) {
        u16 bgaTileIndex = ensureTilesetLoaded(level->bgaTileset);
        bgMap = MAP_create(level->bga, BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, bgaTileIndex));
    } else {
        bgMap = NULL;
    }

    if(level->bgbTileset && level->bgb) {
        u16 bgbTileIndex = ensureTilesetLoaded(level->bgbTileset);
        bgbMap = MAP_create(level->bgb, BG_B, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, bgbTileIndex));
    } else {
        bgbMap = NULL;
    }

    // Mettre à jour la largeur de la map en pixels
    mapWidth = level->mapWidth * 8;
    mapHeight = level->mapHeight * 8;
    return 0;
}

// Helpers: ensure a tileset is loaded once and return its VRAM index
u16 ensureTilesetLoaded(const TileSet* ts) {
    for(u8 i = 0; i < loadedTileSetCount; i++) {
        if(loadedTileSets[i].ts == ts) return loadedTileSets[i].index;
    }
    if(loadedTileSetCount >= MAX_LOADED_TILESETS) return TILE_USER_INDEX;
    u16 idx = VDPTilesFilled;
    VDP_loadTileSet(ts, idx, DMA);
    loadedTileSets[loadedTileSetCount].ts = ts;
    loadedTileSets[loadedTileSetCount].index = idx;
    loadedTileSetCount++;
    VDPTilesFilled += ts->numTile;
    return idx;
}

// Helpers: lightweight tracking for sprite gfx (do not access internal members)
u16 ensureGfxLoaded(const SpriteDefinition* spr) {
    for(u8 i = 0; i < loadedGfxCount; i++) {
        if(loadedGfx[i].gfx == (const void*)spr) return loadedGfx[i].index;
    }
    if(loadedGfxCount >= MAX_LOADED_GFX) return TILE_USER_INDEX;
    // Fallback: do not attempt to load sprite internal gfx (unknown member layout).
    // Return TILE_USER_INDEX as a safe default and track the sprite pointer.
    u16 idx = TILE_USER_INDEX;
    loadedGfx[loadedGfxCount].gfx = (const void*)spr;
    loadedGfx[loadedGfxCount].index = idx;
    loadedGfxCount++;
    return idx;
}

void initGame() {
    // Initialiser le joueur dans une zone autorisée (tuiles avec valeur 1)
    // Y = 120 + 24 pixels (pieds) = 144 / 8 = tuile 18, qui est dans la zone marchable
    // Ensure player gfx is loaded once and get its VRAM index
    u16 playerTileIndex = ensureGfxLoaded(&sprite_player);
    if(player.sprite == NULL) {
        player.sprite = SPR_addSprite(&sprite_player, 80, 70, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, playerTileIndex));
    } else {
        SPR_setPosition(player.sprite, 80, 70);
    }
    player.x = 80;
    player.y = 70;
    player.groundY = 120;
    player.velX = 0;
    player.velY = 0;
    player.facingRight = TRUE;
    player.shootCooldown = 0;
    player.shootAnimTimer = 0;
    player.onGround = TRUE;
    player.blockHorizDuringJump = FALSE;
    player.hp = PLAYER_MAX_HP;
    player.maxHp = PLAYER_MAX_HP;
    player.invincible = FALSE;
    player.invincibleTimer = 0;
    SPR_setAnim(player.sprite, PLAYERANIM_IDLE);
    
    // Initialiser les projectiles
    u16 bulletTileIndex = ensureGfxLoaded(&sprite_player_bullet);
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].sprite == NULL) {
            bullets[i].sprite = SPR_addSprite(&sprite_player_bullet, -32, -32, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, bulletTileIndex));
        } else {
            SPR_setPosition(bullets[i].sprite, -32, -32);
        }
        bullets[i].active = FALSE;
        bullets[i].exploding = FALSE;
        bullets[i].explodeTimer = 0;
        SPR_setVisibility(bullets[i].sprite, HIDDEN);
    }
    
    // Initialiser les ennemis
    // Do not pre-create enemy sprites here. We'll create a sprite per enemy
    // when spawning so different enemy types can use different sprite defs.
    Level* level = &levels[currentLevel];
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = FALSE;
        if(enemies[i].sprite) {
            SPR_setPosition(enemies[i].sprite, -32, -32);
            SPR_setVisibility(enemies[i].sprite, HIDDEN);
        } else {
            enemies[i].sprite = NULL;
        }
    }
    
    // Spawner les ennemis du niveau actuel
        for(u8 i = 0; i < level->enemyCount && i < MAX_ENEMIES; i++) {
            u8 etype = 0;
            if(i < MAX_ENEMIES) etype = level->enemySpawnType[i];
            spawnEnemy(level->enemySpawnX[i], level->enemySpawnY[i], etype);
        }
}

void handleInput() {
    u16 joy = JOY_readJoypad(JOY_1);
    
    player.velX = 0;
    u8 jumpInitiated = FALSE;
        if(joy & BUTTON_C) {
            initLevels();
            loadLevel(currentLevel+1,VDPTilesFilled);  // Charger le niveau 2 (index 1)
            initGame();
        }
    // Saut avec B - vérifier en premier avant tout autre déplacement
    if((joy & BUTTON_B) && player.onGround) {
        player.groundY = player.y;  // Sauvegarder la position Y actuelle avant le saut
        player.velY = PLAYER_JUMP_POWER;
        player.onGround = FALSE;
        jumpInitiated = TRUE;

        // If player is attempting to jump toward a direction (left/right),
        // check the adjacent tile at foot level. If that tile is 0 (solid wall),
        // block horizontal movement during this jump initiation.
        u8 leftSolid = FALSE, rightSolid = FALSE;
        checkAdjacentSolids(player.x, player.y, &leftSolid, &rightSolid);

        // Block horizontal input while airborne if the player is adjacent to
        // a solid tile in the direction they're facing or pressing.
        if((leftSolid && (joy & BUTTON_LEFT)) || (leftSolid && !player.facingRight) ||
           (rightSolid && (joy & BUTTON_RIGHT)) || (rightSolid && player.facingRight)) {
            player.blockHorizDuringJump = TRUE;
        } else {
            player.blockHorizDuringJump = FALSE;
        }
        
        if(player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_JUMP);
        }
    }
    
    // Déplacement horizontal
    if(joy & BUTTON_LEFT) {
        // If a jump was initiated into a wall (or we're mid-air with the flag),
        // block horizontal movement while airborne.
        if(!(player.blockHorizDuringJump && !player.onGround)) {
            player.velX = -PLAYER_SPEED;
            player.facingRight = FALSE;
            if(player.onGround && player.shootAnimTimer == 0) {
                SPR_setAnim(player.sprite, PLAYERANIM_WALK);
            }
        }
    }
    else if(joy & BUTTON_RIGHT) {
        if(!(player.blockHorizDuringJump && !player.onGround)) {
            player.velX = PLAYER_SPEED;
            player.facingRight = TRUE;
            if(player.onGround && player.shootAnimTimer == 0) {
                SPR_setAnim(player.sprite, PLAYERANIM_WALK);
            }
        }
    }
    
    // Déplacement vertical avec haut/bas - seulement si au sol (pas en saut)
    u8 verticalMove = FALSE;
    if(player.onGround) {
        // Déplacement vertical du joueur classique
        if(joy & BUTTON_UP) {
            s16 newY = player.y - PLAYER_SPEED;
            if(newY < 0) newY = 0;
            // Vérifier la collision avec le levelMap
            if(canMoveToPosition(player.x, newY)) {
                player.y = newY;
                verticalMove = TRUE;
                if(player.shootAnimTimer == 0) {
                    SPR_setAnim(player.sprite, PLAYERANIM_WALK);
                }
            }
        }
        else if(joy & BUTTON_DOWN) {
            s16 newY = player.y + PLAYER_SPEED;
            if(newY > GROUND_Y) newY = GROUND_Y;
            // Empêcher le joueur de descendre en dessous de la ligne 20
            if(newY + 24 > 160) newY = 160 - 24;
            // Vérifier la collision avec le levelMap
            if(canMoveToPosition(player.x, newY)) {
                player.y = newY;
                verticalMove = TRUE;
                if(player.shootAnimTimer == 0) {
                    SPR_setAnim(player.sprite, PLAYERANIM_WALK);
                }
            }
        }
    }
    
    // Si aucun mouvement, idle
    if(player.velX == 0 && !verticalMove) {
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_IDLE);
        }
    }
    
    // Tir avec A
    if(joy & BUTTON_A) {
        if(player.shootCooldown == 0) {
            shootBullet();
            player.shootCooldown = SHOOT_COOLDOWN;
            player.shootAnimTimer = 10;
            SPR_setAnim(player.sprite, PLAYERANIM_SHOOT);
        }
    }
    
    SPR_setHFlip(player.sprite, !player.facingRight);
}

void updatePlayer() {
    // Gravité et saut en premier
    if(!player.onGround) {
        player.velY += GRAVITY;
        s16 newY = player.y + player.velY;
        
        // Empêcher de descendre en dessous de la ligne 20
        if(newY + 24 > 160) {
            newY = 160 - 24;
            player.velY = 0;
            player.onGround = TRUE;
            player.y = newY;
            player.groundY = newY;
            player.blockHorizDuringJump = FALSE;
        }
        // Toujours appliquer le mouvement vertical
        else {
            player.y = newY;
            
            // Si on tombe (velY >= 0), vérifier si on a atteint ou dépassé la position au sol
            if(player.velY >= 0 && player.y >= player.groundY) {
                player.y = player.groundY;
                player.onGround = TRUE;
                player.velY = 0;
                player.blockHorizDuringJump = FALSE;
            }
        }
        
        // Animation de chute
        if(player.velY > 0 && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_FALL);
        }
    }
    
    // Déplacement horizontal avec collision
    if(player.velX != 0) {
        s16 newX = player.x + player.velX;
        
        // Limites de déplacement horizontal
        if(newX < 0) newX = 0;
        if(newX > mapWidth - 16) newX = mapWidth - 16;
        
        // Vérifier la collision avec le levelMap seulement si au sol
        // En l'air, on peut se déplacer librement horizontalement
        if(!player.onGround) {
            player.x = newX;
        } else if(canMoveToPosition(newX, player.y)) {
            player.x = newX;
        }
    }

    // Empêcher le joueur de sortir de la zone visible lorsque la caméra ne recule pas
    // (la caméra n'est que monotone vers la droite). On clamp en coordonnées monde
    // pour garder le joueur visible: [cameraX, cameraX + SCREEN_WIDTH - spriteWidth]
    {
        const s16 spriteWidth = 16;
        s16 minVisibleX = cameraX;
        s16 maxVisibleX = cameraX + SCREEN_WIDTH - spriteWidth;
        if(player.x < minVisibleX) player.x = minVisibleX;
        if(player.x > maxVisibleX) player.x = maxVisibleX;
    }

    // (BG_A vertical adjustment moved to updateCamera so it only
    // happens when the camera actually moves forward)
    
    if(player.shootCooldown > 0) {
        player.shootCooldown--;
    }
    
    // Gérer l'invincibilité
    if(player.invincible) {
        player.invincibleTimer--;
        if(player.invincibleTimer == 0) {
            player.invincible = FALSE;
        }
    }
    
    // Gérer le timer de l'animation de tir
    if(player.shootAnimTimer > 0) {
        player.shootAnimTimer--;
        if(player.shootAnimTimer == 0) {
            // Retour à l'animation appropriée
            if(!player.onGround) {
                if(player.velY > 0) {
                    SPR_setAnim(player.sprite, PLAYERANIM_FALL);
                } else {
                    SPR_setAnim(player.sprite, PLAYERANIM_JUMP);
                }
            } else if(player.velX != 0) {
                SPR_setAnim(player.sprite, PLAYERANIM_WALK);
            } else {
                SPR_setAnim(player.sprite, PLAYERANIM_IDLE);
            }
        }
    }
}

void shootBullet() {
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(!bullets[i].active) {
            bullets[i].active = TRUE;
            bullets[i].exploding = FALSE;
            bullets[i].explodeTimer = 0;
            bullets[i].x = player.x + (player.facingRight ? 16 : -16);
            bullets[i].y = player.y + 8;
            bullets[i].startX = bullets[i].x;
            bullets[i].facingRight = player.facingRight;
            SPR_setVisibility(bullets[i].sprite, VISIBLE);
            SPR_setAnim(bullets[i].sprite, PLAYERANIM_BULLET);
            SPR_setFrame(bullets[i].sprite, 0);
            SPR_setHFlip(bullets[i].sprite, !bullets[i].facingRight);
            break;
        }
    }
}

void updateBullets() {
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].active) {
            if(bullets[i].exploding) {
                // Gérer l'animation d'explosion
                bullets[i].explodeTimer--;
                if(bullets[i].explodeTimer == 0) {
                    // Fin de l'explosion, désactiver la balle
                    bullets[i].active = FALSE;
                    bullets[i].exploding = FALSE;
                    SPR_setVisibility(bullets[i].sprite, HIDDEN);
                }
            } else {
                // Déplacement normal de la balle
                if(bullets[i].facingRight) {
                    bullets[i].x += BULLET_SPEED;
                } else {
                    bullets[i].x -= BULLET_SPEED;
                }
                
                // Calculer la distance parcourue
                s16 distance = abs(bullets[i].x - bullets[i].startX);
                
                // Désactiver si distance max atteinte ou hors écran
                if(distance >= BULLET_MAX_DISTANCE || bullets[i].x < cameraX - 32 || bullets[i].x > cameraX + SCREEN_WIDTH + 32) {
                    bullets[i].active = FALSE;
                    SPR_setVisibility(bullets[i].sprite, HIDDEN);
                }
            }
        }
    }
}

void updateEnemies() {
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active) {
            if(enemies[i].dying) {
                // Gérer l'animation de mort
                enemies[i].deathTimer--;
                if(enemies[i].deathTimer == 0) {
                    enemies[i].active = FALSE;
                    SPR_setVisibility(enemies[i].sprite, HIDDEN);
                }
                continue;
            }
            
            // Flying enemies: hover, dive to player when in range, then return
            if(enemies[i].type == ENEMY_TYPE_FLYING) {
                s16 fdistX = player.x - enemies[i].x;
                s16 fdistY = player.y - enemies[i].y;
                // simple hover: use aiTimer as phase
                enemies[i].aiTimer++;
                s16 phase = (enemies[i].aiTimer & 31) - 16; // -16..15
                s16 hover = phase / 4; // -4..3 pixels

                // If currently in dive state
                if(enemies[i].diveState == 1) {
                    if(enemies[i].diveTimer > 0) {
                        enemies[i].diveTimer--;
                        // Fast dive toward player
                        s16 sx = (fdistX > 0) ? enemies[i].speed * 3 : -enemies[i].speed * 3;
                        enemies[i].x += sx;
                        if(player.y > enemies[i].y) enemies[i].y += enemies[i].speed * 3;
                        else enemies[i].y -= enemies[i].speed * 3;
                        enemies[i].facingRight = (fdistX > 0);
                        SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                    } else {
                        // Dive finished: start returning phase
                        enemies[i].diveState = 2;
                        enemies[i].attackCooldown = 90;
                        enemies[i].attacking = FALSE;
                        SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                    }
                }
                else if(enemies[i].diveState == 2) {
                    // Return to base hover position
                    s16 targetY = enemies[i].baseY + hover;
                    s16 dy = targetY - enemies[i].y;
                    if(abs(dy) > enemies[i].speed) enemies[i].y += (dy > 0) ? enemies[i].speed : -enemies[i].speed;
                    else enemies[i].y = targetY;

                    s16 dx = enemies[i].baseX - enemies[i].x;
                    if(abs(dx) > enemies[i].speed) {
                        enemies[i].x += (dx > 0) ? enemies[i].speed : -enemies[i].speed;
                        enemies[i].facingRight = (dx > 0) ? TRUE : FALSE;
                    } else {
                        enemies[i].x = enemies[i].baseX;
                    }

                    // Face toward base while returning; switch to idle when done
                    SPR_setHFlip(enemies[i].sprite, !enemies[i].facingRight);
                    if(abs(dy) <= 1 && abs(dx) <= 1) {
                        enemies[i].diveState = 0;
                        SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
                    }
                }
                else {
                    // Idle hover / approach
                    if(abs(fdistX) < enemies[i].attackRange && abs(fdistY) < 40) {
                        // If player is below and roughly within range, start a dive
                        if(player.y > enemies[i].y + 8 && abs(fdistX) < enemies[i].attackRange) {
                            enemies[i].diveState = 1;
                            enemies[i].diveTimer = 24; // frames for dive
                            enemies[i].attacking = TRUE;
                            SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                        } else if(!enemies[i].attacking) {
                            enemies[i].attacking = TRUE;
                            enemies[i].attackCooldown = 60;
                            SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                        }
                    } else if(!enemies[i].attacking) {
                        // Move towards/strafe around player a bit while hovering
                        s16 nx = enemies[i].x;
                        s16 ny = enemies[i].baseY + hover;
                        if(abs(fdistX) > 8) {
                            nx += (fdistX > 0) ? enemies[i].speed : -enemies[i].speed;
                            enemies[i].facingRight = (fdistX > 0) ? TRUE : FALSE;
                        } else {
                            // small strafe motion
                            nx += (enemies[i].aiTimer & 1) ? enemies[i].speed : -enemies[i].speed;
                        }
                        enemies[i].x = nx;
                        enemies[i].y = ny;
                        SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                    }
                }
                // Flying enemies do not use gravity
            }
            else {
                // Appliquer la gravité si l'ennemi n'est pas au sol
                if(!enemies[i].onGround) {
                    enemies[i].velY += GRAVITY;
                    enemies[i].y += enemies[i].velY;
                    
                    // Vérifier si l'ennemi atteint une zone marchable (tile 1)
                    if(canMoveToPosition(enemies[i].x, enemies[i].y)) {
                        enemies[i].onGround = TRUE;
                        enemies[i].velY = 0;
                    } else if(enemies[i].y + 24 > 160) {
                        // Limite de la ligne 20
                        enemies[i].y = 136;
                        enemies[i].onGround = TRUE;
                        enemies[i].velY = 0;
                    }
                }
            }
            
            s16 distX = player.x - enemies[i].x;
            s16 distY = player.y - enemies[i].y;
            
            // Décrémenter le cooldown
            if(enemies[i].attackCooldown > 0) {
                enemies[i].attackCooldown--;
            }
            
            // Si le joueur est très loin, réinitialiser l'état
            if(abs(distX) > 250 || abs(distY) > 100) {
                enemies[i].attacking = FALSE;
                if(enemies[i].attackCooldown == 0) {
                    SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
                }
            }
            
            // Logique d'action uniquement si pas de cooldown
            if(enemies[i].attackCooldown == 0) {
                // Behavior by type
                if(enemies[i].type == ENEMY_TYPE_TANK) {
                    // Tank: if within charge trigger, start charging
                    if(abs(distX) < 80 && !enemies[i].attacking) {
                        // begin charge
                        enemies[i].attacking = TRUE;
                        enemies[i].aiTimer = 30; // charge duration
                        enemies[i].attackCooldown = 90;
                        SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                    }
                    // during charge, move faster
                    if(enemies[i].attacking && enemies[i].aiTimer > 0) {
                        enemies[i].aiTimer--;
                        s16 step = enemies[i].speed * 2;
                        if(distX > 0) enemies[i].x += step, enemies[i].facingRight = TRUE;
                        else enemies[i].x -= step, enemies[i].facingRight = FALSE;
                    } else {
                        // slow approach when not charging
                        if(abs(distX) < 200) {
                            SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                            if(abs(distY) > 5) {
                                enemies[i].y += (distY > 0) ? enemies[i].speed : -enemies[i].speed;
                            } else if(abs(distX) > 10) {
                                enemies[i].x += (distX > 0) ? enemies[i].speed : -enemies[i].speed;
                                enemies[i].facingRight = (distX > 0);
                            }
                        }
                    }
                }
                else if(enemies[i].type == ENEMY_TYPE_RANGE) {
                    // Ranged: keep distance, retreat if too close.
                    // Align vertically with player before attacking.
                    s16 preferred = enemies[i].attackRange;
                    if(abs(distX) < preferred - 20) {
                        // too close: retreat horizontally
                        enemies[i].x += (distX > 0) ? -enemies[i].speed : enemies[i].speed;
                        SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                    } else if(abs(distX) > preferred + 10 && abs(distX) < 220) {
                        // approach to get into range
                        enemies[i].x += (distX > 0) ? enemies[i].speed : -enemies[i].speed;
                        enemies[i].facingRight = (distX > 0);
                        SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                    } else {
                        // In horizontal optimal range: first try to align vertically
                        // before starting the attack.
                        const s16 yAlignThreshold = 6;
                        if(abs(distY) > yAlignThreshold) {
                            // Move vertically toward the player
                            if(distY > 0) enemies[i].y += enemies[i].speed;
                            else enemies[i].y -= enemies[i].speed;
                            SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                        } else {
                            // Aligned vertically: perform attack
                            enemies[i].attacking = TRUE;
                            enemies[i].attackCooldown = 60;
                            SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                        }
                    }
                }
                else {
                    // Default melee soldier behavior
                    // Attaque si très proche (même X et même Y)
                    if(abs(distX) < ENEMY_ATTACK_RANGE && abs(distY) < 10) {
                        if(!enemies[i].attacking) {
                            enemies[i].attacking = TRUE;
                            enemies[i].attackCooldown = 60; // Cooldown après attaque
                            SPR_setAnim(enemies[i].sprite, EANIM_ATTACK);
                        }
                    }
                    // Poursuivre le joueur
                    else if(abs(distX) < 200 && !enemies[i].attacking) {
                        SPR_setAnim(enemies[i].sprite, EANIM_WALK);
                        
                        s16 newX = enemies[i].x;
                        s16 newY = enemies[i].y;
                        
                        // Priorité 1: Se mettre au même niveau Y que le joueur
                        if(abs(distY) > 5) {
                            if(distY > 0) {
                                newY += ENEMY_SPEED;
                            } else {
                                newY -= ENEMY_SPEED;
                            }
                        }
                        // Priorité 2: Se rapprocher horizontalement une fois aligné verticalement
                        else if(abs(distX) > 10) {
                            if(distX > 0) {
                                newX += ENEMY_SPEED;
                                enemies[i].facingRight = TRUE;
                            } else {
                                newX -= ENEMY_SPEED;
                                enemies[i].facingRight = FALSE;
                            }
                        }

                        // Vérifier les collisions avec les autres ennemis
                        u8 collision = FALSE;
                        for(u8 j = 0; j < MAX_ENEMIES; j++) {
                            if(j != i && enemies[j].active && !enemies[j].dying) {
                                s16 dx = newX - enemies[j].x;
                                s16 dy = newY - enemies[j].y;
                                // Collision si distance < 20 pixels
                                if(abs(dx) < 20 && abs(dy) < 20) {
                                    collision = TRUE;
                                    break;
                                }
                            }
                        }

                        // Appliquer le déplacement seulement s'il n'y a pas de collision
                        if(!collision) {
                            enemies[i].x = newX;
                            enemies[i].y = newY;
                        }
                    }
                    else {
                        // Idle
                        if(!enemies[i].attacking) {
                            SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
                        }
                    }
                }
            }
            else {
                // Cooldown actif, rester idle
                if(!enemies[i].attacking) {
                    SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
                }
            }
            
            // Si en attaque et joueur s'éloigne, arrêter l'attaque
            if(enemies[i].attacking && (abs(distX) >= ENEMY_ATTACK_RANGE || abs(distY) >= 20)) {
                enemies[i].attacking = FALSE;
            }
            
            // Limiter le Y de l'ennemi
            if(enemies[i].y < 0) enemies[i].y = 0;
            if(enemies[i].y > GROUND_Y) enemies[i].y = GROUND_Y;
            
            SPR_setHFlip(enemies[i].sprite, !enemies[i].facingRight);
        }
    }
}

void updateCamera() {
    // Caméra suit le joueur
    s16 targetCameraX = player.x - SCREEN_WIDTH / 2;

    // Limites de la caméra horizontale
    if(targetCameraX < 0) targetCameraX = 0;
    if(targetCameraX > mapWidth - SCREEN_WIDTH) targetCameraX = mapWidth - SCREEN_WIDTH;
    // Ne pas reculer la caméra : n'accepter que les augmentations de targetCameraX
    s16 prevCameraX = cameraX;
    if(targetCameraX > cameraX) {
        cameraX = targetCameraX;
    }

    // Désactiver le scroll vertical : garder la caméra verticale fixe
    cameraY = 0;
    bgbCameraY = 0;

    // Exemple: le plan B peut avoir un offset vertical différent (parallaxe)
    bgbCameraY = cameraY / 2;

    // Ajuster le scroll vertical de BG_A uniquement si la caméra a avancé
    // (scroll down only when camera moves forward)
    {
        static u8 bgaScrollCounter = 0;
        Level* lvl = &levels[currentLevel];
        // Calculer la limite maximale d'offset pour BG_A en pixels.
        // On limite par la hauteur de la map (en tuiles * 8) moins la hauteur d'écran.
        s16 maxAllowed = (s16)lvl->mapHeight * 8 - SCREEN_HEIGHT;
        if(maxAllowed < 0) maxAllowed = 0;
        // Si un `bgaMaxOffsetY` spécifique est fourni et plus petit, l'appliquer.
        if(lvl->bgaMaxOffsetY > 0 && lvl->bgaMaxOffsetY < maxAllowed) {
            maxAllowed = lvl->bgaMaxOffsetY;
        }
        if(cameraX > prevCameraX) {
            bgaScrollCounter++;
            u8 delay = (lvl->bgaScrollDelay > 0) ? lvl->bgaScrollDelay : BGA_SCROLL_FRAME_DELAY;
            if(bgaScrollCounter >= delay) {
                // inverted movement: on camera advance, move BGA in opposite direction
                if(lvl->bgaScrollStep > 0) {
                    s16 step = lvl->bgaScrollStep;
                    if(lvl->bgaOffsetY > 0) {
                        lvl->bgaOffsetY -= step;
                        if(lvl->bgaOffsetY < 0) lvl->bgaOffsetY = 0;
                        if(lvl->bgaOffsetY > maxAllowed) lvl->bgaOffsetY = maxAllowed;
                    }
                }
                bgaScrollCounter = 0;
            }
        } else {
            // reset counter when camera not advancing
            bgaScrollCounter = 0;
        }
    }

    // Ajuster le scroll vertical de BG_B de la même façon que BG_A (même delay)
    {
        static u8 bgbScrollCounter = 0;
        Level* lvlb = &levels[currentLevel];
        // calculer une limite raisonnable pour BG_B basée sur la hauteur de la map
        s16 maxBgbAllowed = (s16)lvlb->mapHeight * 8 - SCREEN_HEIGHT;
        if(maxBgbAllowed < 0) maxBgbAllowed = 0;
        if(cameraX > prevCameraX) {
            bgbScrollCounter++;
            u8 delayb = (lvlb->bgbScrollDelay > 0) ? lvlb->bgbScrollDelay : BGA_SCROLL_FRAME_DELAY;
            if(bgbScrollCounter >= delayb) {
                // inverted movement: on camera advance, move BGB in opposite direction
                if(lvlb->bgbScrollStep > 0) {
                    s16 stepb = lvlb->bgbScrollStep;
                    if(lvlb->bgbOffsetY > 0) {
                        lvlb->bgbOffsetY -= stepb;
                    }
                    if(lvlb->bgbOffsetY < 0) lvlb->bgbOffsetY = 0;
                    if(lvlb->bgbOffsetY > maxBgbAllowed) lvlb->bgbOffsetY = maxBgbAllowed;
                }
                bgbScrollCounter = 0;
            }
        } else {
            bgbScrollCounter = 0;
        }
    }

    // Mettre à jour la position du sprite du joueur à l'écran
    SPR_setPosition(player.sprite, player.x - cameraX, player.y - cameraY);

    // Mettre à jour les projectiles
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].active) {
            SPR_setPosition(bullets[i].sprite, bullets[i].x - cameraX, bullets[i].y - cameraY);
        }
    }

    // Mettre à jour les ennemis
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active) {
            s16 screenX = enemies[i].x - cameraX;
            s16 screenY = enemies[i].y - cameraY;
            if(screenX > -32 && screenX < SCREEN_WIDTH + 32 && screenY > -32 && screenY < SCREEN_HEIGHT + 32) {
                SPR_setPosition(enemies[i].sprite, screenX, screenY);
                SPR_setVisibility(enemies[i].sprite, VISIBLE);
            } else {
                SPR_setVisibility(enemies[i].sprite, HIDDEN);
            }
        }
    }
    // Scroll du background à la même vitesse que la caméra + offset par niveau
    // Scroll du background à la même vitesse que la caméra + offset par niveau
    // (l'offset vertical `bgaOffsetY` est géré depuis `updatePlayer()`)
    MAP_scrollTo(bgMap, cameraX, cameraY + levels[currentLevel].bgaOffsetY);

    // Scroll du plan B avec effet parallaxe + offset par niveau
    if(bgbMap != NULL) {
        MAP_scrollTo(bgbMap, cameraX / 2, bgbCameraY + levels[currentLevel].bgbOffsetY);
    }
}
void checkCollisions() {
    // Collision projectiles -> ennemis
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].active && !bullets[i].exploding) {
            for(u8 j = 0; j < MAX_ENEMIES; j++) {
                if(enemies[j].active && !enemies[j].dying) {
                    // Calculer la position de la hitbox de l'ennemi
                    s16 enemyHitboxX = enemies[j].x + enemies[j].hitboxOffsetX;
                    s16 enemyHitboxY = enemies[j].y + enemies[j].hitboxOffsetY;
                    
                    // Vérifier la collision avec la hitbox
                    if(bullets[i].x >= enemyHitboxX && 
                       bullets[i].x <= enemyHitboxX + enemies[j].hitboxWidth &&
                       bullets[i].y >= enemyHitboxY && 
                       bullets[i].y <= enemyHitboxY + enemies[j].hitboxHeight) {
                        // Touché! Déclencher l'animation d'explosion de la bullet
                        bullets[i].exploding = TRUE;
                        bullets[i].explodeTimer = 10;
                        SPR_setAnim(bullets[i].sprite, PLAYERANIM_BULLET_HIT);
                        
                        // Infliger des dégâts à l'ennemi
                        enemies[j].hp -= BULLET_DAMAGE;
                        lastHitEnemyIndex = j;
                        lastHitEnemyDisplayTimer = 120; // Afficher pendant 2 secondes
                        if(enemies[j].hp <= 0) {
                            // Déclencher l'animation de mort de l'ennemi
                            enemies[j].dying = TRUE;
                            enemies[j].deathTimer = 30;
                            SPR_setAnim(enemies[j].sprite, EANIM_DEATH);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Collision ennemis -> joueur
    if(!player.invincible) {
        for(u8 i = 0; i < MAX_ENEMIES; i++) {
            if(enemies[i].active && !enemies[i].dying && enemies[i].attacking) {
                s16 dx = player.x - enemies[i].x;
                s16 dy = player.y - enemies[i].y;
                
                // Si l'ennemi est en attaque et proche
                if(abs(dx) < 20 && abs(dy) < 20) {
                    player.hp -= ENEMY_DAMAGE;
                    player.invincible = TRUE;
                    player.invincibleTimer = 60; // 1 seconde d'invincibilité
                    
                    if(player.hp <= 0) {
                        player.hp = 0;
                        // Game over logic ici
                    }
                    break;
                }
            }
        }
    }
}

void spawnEnemy(s16 x, s16 y, u8 type) {
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(!enemies[i].active) {
            enemies[i].active = TRUE;
            enemies[i].type = type;
            enemies[i].x = x;
            // Place non-flying enemies on a walkable tile (tile value 1).
            Level* level = &levels[currentLevel];
            s16 spawnY = y;
            if(type != ENEMY_TYPE_FLYING && level->collisionMap) {
                // Determine tile X for this spawn X
                s16 tileX = x / 8;
                if(tileX < 0) tileX = 0;
                if(tileX >= level->mapWidth) tileX = level->mapWidth - 1;

                s16 foundY = -1;
                for(s16 tileFeetY = 0; tileFeetY < level->mapHeight; tileFeetY++) {
                    // Convert desired feet tile row back to sprite Y
                    s16 candidateY = tileFeetY * 8 - 24 - level->bgaOffsetY;
                    if(canMoveToPosition(x, candidateY)) {
                        foundY = candidateY;
                        break;
                    }
                }
                if(foundY >= 0) spawnY = foundY;
                else {
                    // fallback: place near bottom of map
                    spawnY = (level->mapHeight * 8) - 24 - level->bgaOffsetY;
                    if(spawnY < 0) spawnY = 0;
                }
            }
            enemies[i].y = spawnY;
            enemies[i].velY = 0;
            // Flying enemies should not be affected by gravity; others start on ground or in-air
            if(type == ENEMY_TYPE_FLYING) {
                enemies[i].onGround = FALSE; // treat as airborne but skip gravity
            } else {
                enemies[i].onGround = FALSE;
            }
            enemies[i].facingRight = FALSE;
            enemies[i].attacking = FALSE;
            enemies[i].attackCooldown = 0;
            enemies[i].dying = FALSE;
            enemies[i].deathTimer = 0;
            // Initialiser la hitbox (centre du sprite)
            enemies[i].hitboxOffsetX = 4;
            enemies[i].hitboxOffsetY = 4;
            enemies[i].hitboxWidth = 24;
            enemies[i].hitboxHeight = 28;
            // remember base Y and reset AI timer (use actual spawn Y)
            enemies[i].baseY = enemies[i].y;
            enemies[i].baseX = enemies[i].x;
            enemies[i].diveState = 0;
            enemies[i].diveTimer = 0;
            enemies[i].aiTimer = 0;
            // Set per-type stats
            if(type == ENEMY_TYPE_SOLDIER) {
                // Soldier (melee)
                enemies[i].speed = 1;
                enemies[i].attackRange = 20;
                enemies[i].hp = 60;
                enemies[i].maxHp = 60;
                enemies[i].damage = 10;
            } else if(type == ENEMY_TYPE_RANGE) {
                // Ranged
                enemies[i].speed = 2;
                enemies[i].attackRange = 100;
                enemies[i].hp = 40;
                enemies[i].maxHp = 40;
                enemies[i].damage = 6;
            } else if(type == ENEMY_TYPE_TANK) {
                // Tank (melee heavy)
                enemies[i].speed = 1;
                enemies[i].attackRange = 25;
                enemies[i].hp = 120;
                enemies[i].maxHp = 120;
                enemies[i].damage = 20;
            } else if(type == ENEMY_TYPE_FLYING) {
                // Flying (can move in Y freely)
                enemies[i].speed = 2;
                enemies[i].attackRange = 60;
                enemies[i].hp = 40;
                enemies[i].maxHp = 40;
                enemies[i].damage = 8;
            } else {
                enemies[i].speed = ENEMY_SPEED;
                enemies[i].attackRange = ENEMY_ATTACK_RANGE;
                enemies[i].hp = ENEMY_MAX_HP;
                enemies[i].maxHp = ENEMY_MAX_HP;
                enemies[i].damage = ENEMY_DAMAGE;
            }
            // Create or update the enemy sprite based on its type so ranged
            // enemies can use a different sprite (sprite_range1).
            const SpriteDefinition* def = level->enemySprite ? level->enemySprite : &sprite_soldier;
            if(type == ENEMY_TYPE_RANGE) {
                def = &sprite_range1;
            } else if(type == ENEMY_TYPE_FLYING) {
                def = &sprite_flying1;
            }
            u16 eTileIndex = ensureGfxLoaded(def);
            if(enemies[i].sprite == NULL) {
                enemies[i].sprite = SPR_addSprite(def, enemies[i].x - cameraX, enemies[i].y - cameraY, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, eTileIndex));
            } else {
                // Reposition existing sprite. If it uses a different gfx, that's
                // acceptable for now; creating/destroying sprites at runtime can
                // be platform-dependent.
                SPR_setPosition(enemies[i].sprite, enemies[i].x - cameraX, enemies[i].y - cameraY);
                SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
                SPR_setFrame(enemies[i].sprite, 0);
                SPR_setVisibility(enemies[i].sprite, VISIBLE);
            }
            SPR_setVisibility(enemies[i].sprite, VISIBLE);
            SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
            break;
        }
    }
}

// Fonction pour vérifier si le joueur peut se déplacer à une position donnée
// Retourne TRUE si la tuile à cette position est une tuile marchable (valeur 1)
u8 canMoveToPosition(s16 x, s16 y) {
    // Vérifier la collision au niveau des pieds du joueur (bas du sprite)
    // En supposant que le sprite fait environ 24 pixels de haut et 16 pixels de large
    // Prendre en compte l'offset vertical appliqué au BG_A pour l'affichage
    // afin que la map de collision corresponde au rendu visuel.
    s16 feetY = y + 24 + levels[currentLevel].bgaOffsetY;
    
    // Vérifier plusieurs points en largeur du sprite (gauche, centre, droite)
    s16 leftX = x;
    s16 centerX = x + 8;
    s16 rightX = x + 15;
    
    // Convertir les coordonnées pixel en coordonnées de tuile pour chaque point
    s16 tileFeetY = feetY / 8;
    
    // Utiliser la collision map du niveau actuel
    const u16* currentMap = levels[currentLevel].collisionMap;
    u16 currentMapWidth = levels[currentLevel].mapWidth;
    u16 currentMapHeight = levels[currentLevel].mapHeight;
    
    // Vérifier le point gauche
    s16 tileLeftX = leftX / 8;
    if(tileLeftX >= 0 && tileLeftX < currentMapWidth && tileFeetY >= 0 && tileFeetY < currentMapHeight) {
        u16 index = tileFeetY * currentMapWidth + tileLeftX;
        if(currentMap[index] != 1) return FALSE;
    } else {
        return FALSE;
    }
    
    // Vérifier le point centre
    s16 tileCenterX = centerX / 8;
    if(tileCenterX >= 0 && tileCenterX < currentMapWidth && tileFeetY >= 0 && tileFeetY < currentMapHeight) {
        u16 index = tileFeetY * currentMapWidth + tileCenterX;
        if(currentMap[index] != 1) return FALSE;
    } else {
        return FALSE;
    }
    
    // Vérifier le point droite
    s16 tileRightX = rightX / 8;
    if(tileRightX >= 0 && tileRightX < currentMapWidth && tileFeetY >= 0 && tileFeetY < currentMapHeight) {
        u16 index = tileFeetY * currentMapWidth + tileRightX;
        if(currentMap[index] != 1) return FALSE;
    } else {
        return FALSE;
    }
    
    return TRUE;
}

// Helper: check if tiles adjacent to player at feet and mid-height are solid (value 0)
void checkAdjacentSolids(s16 px, s16 py, u8* leftSolid, u8* rightSolid) {
    *leftSolid = FALSE;
    *rightSolid = FALSE;
    const u16* currentMap = levels[currentLevel].collisionMap;
    u16 currentMapWidth = levels[currentLevel].mapWidth;
    u16 currentMapHeight = levels[currentLevel].mapHeight;

    s16 feetY = py + 24 + levels[currentLevel].bgaOffsetY;
    s16 midY = py + 12 + levels[currentLevel].bgaOffsetY;
    s16 tileFeetY = feetY / 8;
    s16 tileMidY = midY / 8;

    s16 leftTileX = (px - 1) / 8;
    s16 rightTileX = (px + 16) / 8;

    if(leftTileX >= 0 && leftTileX < currentMapWidth) {
        if(tileFeetY >= 0 && tileFeetY < currentMapHeight) {
            u16 idx = tileFeetY * currentMapWidth + leftTileX;
            if(currentMap[idx] == 0) *leftSolid = TRUE;
        }
        if(!*leftSolid && tileMidY >= 0 && tileMidY < currentMapHeight) {
            u16 idx = tileMidY * currentMapWidth + leftTileX;
            if(currentMap[idx] == 0) *leftSolid = TRUE;
        }
    }

    if(rightTileX >= 0 && rightTileX < currentMapWidth) {
        if(tileFeetY >= 0 && tileFeetY < currentMapHeight) {
            u16 idx = tileFeetY * currentMapWidth + rightTileX;
            if(currentMap[idx] == 0) *rightSolid = TRUE;
        }
        if(!*rightSolid && tileMidY >= 0 && tileMidY < currentMapHeight) {
            u16 idx = tileMidY * currentMapWidth + rightTileX;
            if(currentMap[idx] == 0) *rightSolid = TRUE;
        }
    }
}

void drawHUD() {
    char text[32];
        VDP_setTextPlane(WINDOW);
      //  VDP_drawText("===============================", 0, 24);
      //VDP_drawText("SCORE: 00000    VIES: 3", 2, 25);   
    // Afficher les HP du joueur (seulement si changés)
    if(player.hp != lastPlayerHP) {
        lastPlayerHP = player.hp;
        sprintf(text, "HP: %d/%d", player.hp, player.maxHp);
        VDP_clearText(1, 22, 15);
        VDP_drawText(text, 1, 22);
        
        // Afficher une barre de vie graphique
        u8 barLength = (player.hp * 18) / player.maxHp;
        VDP_drawText("[", 1, 23);
        for(u8 i = 0; i < 18; i++) {
            if(i < barLength) {
                VDP_drawText("=", 2 + i, 23);
            } else {
                VDP_drawText("-", 2 + i, 23);
            }
        }
        VDP_drawText("]", 20, 23);
    }
    
    // Compter les ennemis actifs
    u8 enemyCount = 0;
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active && !enemies[i].dying) {
            enemyCount++;
        }
    }
    
    // Afficher le nombre d'ennemis (seulement si changé)
    if(enemyCount != lastEnemyCount) {
        lastEnemyCount = enemyCount;
        sprintf(text, "Enemies: %d", enemyCount);
        VDP_clearText(23, 22, 15);
        VDP_drawText(text, 23, 22);
    }
    
    // Afficher les HP de l'ennemi touché (seulement si changé)
    s16 currentEnemyHP = -1;
    if(lastHitEnemyDisplayTimer > 0) {
        lastHitEnemyDisplayTimer--;
        if(lastHitEnemyIndex >= 0 && lastHitEnemyIndex < MAX_ENEMIES && 
           enemies[lastHitEnemyIndex].active) {
            currentEnemyHP = enemies[lastHitEnemyIndex].hp;
        }
    }
    
    if(currentEnemyHP != lastDisplayedEnemyHP) {
        lastDisplayedEnemyHP = currentEnemyHP;
        VDP_clearText(23, 3, 15);
        if(currentEnemyHP >= 0) {
            sprintf(text, "Enemy HP: %d", currentEnemyHP);
            VDP_drawText(text, 23, 23);
        }
    }
}
