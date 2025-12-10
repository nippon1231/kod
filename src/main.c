#include <genesis.h>
#include "resources.h"
#include "player.h"
#include "soldier.h"
#include "lvl1.h"
#include "level.h"
// Constantes
#define MAX_BULLETS 5
#define MAX_ENEMIES 8
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224
#define GROUND_Y 180
#define PLAYER_SPEED 2
#define PLAYER_JUMP_POWER -15
#define GRAVITY 1
#define BULLET_SPEED 4
#define BULLET_MAX_DISTANCE 180
#define ENEMY_SPEED 1
#define ENEMY_ATTACK_RANGE 30
#define SHOOT_COOLDOWN 15
#define PLAYER_MAX_HP 100
#define ENEMY_MAX_HP 50
#define BULLET_DAMAGE 25
#define ENEMY_DAMAGE 10

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
    s16 x;
    s16 y;
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
} Enemy;

// Variables globales
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
s16 cameraX = 0;
u16 mapWidth = MAP_WIDTH * 8; // Largeur en pixels (192 tiles * 8)
s16 lastHitEnemyIndex = -1;
u8 lastHitEnemyDisplayTimer = 0;

// Variables pour éviter le clignotement du HUD
s16 lastPlayerHP = -1;
u8 lastEnemyCount = 255;
s16 lastDisplayedEnemyHP = -1;
u16 VDPTilesFilled = TILE_USER_INDEX;

// Variable externe pour la map
extern Map* bgMap;

// Prototypes
void initGame();
void updatePlayer();
void updateBullets();
void updateEnemies();
void updateCamera();
void shootBullet();
void handleInput();
void checkCollisions();
void spawnEnemy(s16 x, s16 y);
void drawHUD();

int main() {

    VDP_setWindowVPos(TRUE, 22);
    VDP_setTextPlane(WINDOW);
    //VDP_drawText("===============================", 0, 24);
    //VDP_drawText("SCORE: 00000    VIES: 3", 2, 25);   
    // Initialisation de base
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    SPR_init();
    
    // Charger les palettes
    PAL_setPalette(PAL1, palette_player.data, CPU);
    PAL_setPalette(PAL2, palette_soldier.data, CPU);
    
    // Définir la couleur de fond (backdrop) en bleu
    VDP_setBackgroundColor(16); // Index 16 de la palette (bleu dans la palette par défaut)
    
    initlevel();
    
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

void initGame() {
    // Initialiser le joueur
    player.sprite = SPR_addSprite(&sprite_player, 80, GROUND_Y, TILE_ATTR(PAL1, 0, FALSE, FALSE));
    player.x = 80;
    player.y = GROUND_Y;
    player.groundY = GROUND_Y;
    player.velX = 0;
    player.velY = 0;
    player.facingRight = TRUE;
    player.shootCooldown = 0;
    player.shootAnimTimer = 0;
    player.onGround = TRUE;
    player.hp = PLAYER_MAX_HP;
    player.maxHp = PLAYER_MAX_HP;
    player.invincible = FALSE;
    player.invincibleTimer = 0;
    SPR_setAnim(player.sprite, PLAYERANIM_IDLE);
    
    // Initialiser les projectiles
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        bullets[i].sprite = SPR_addSprite(&sprite_player_bullet, -32, -32, TILE_ATTR(PAL1, 0, FALSE, FALSE));
        bullets[i].active = FALSE;
        bullets[i].exploding = FALSE;
        bullets[i].explodeTimer = 0;
        SPR_setVisibility(bullets[i].sprite, HIDDEN);
    }
    
    // Initialiser les ennemis
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = FALSE;
        enemies[i].sprite = SPR_addSprite(&sprite_soldier, -32, -32, TILE_ATTR(PAL2, 0, FALSE, FALSE));
        SPR_setVisibility(enemies[i].sprite, HIDDEN);
    }
    
    // Spawner quelques ennemis
    spawnEnemy(300, GROUND_Y);
    spawnEnemy(450, GROUND_Y);
    spawnEnemy(600, GROUND_Y);
    spawnEnemy(800, GROUND_Y);
    spawnEnemy(950, GROUND_Y);
}

void handleInput() {
    u16 joy = JOY_readJoypad(JOY_1);
    
    player.velX = 0;
    s16 manualVelY = 0;
    
    // Déplacement horizontal
    if(joy & BUTTON_LEFT) {
        player.velX = -PLAYER_SPEED;
        player.facingRight = FALSE;
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_WALK);
        }
    }
    else if(joy & BUTTON_RIGHT) {
        player.velX = PLAYER_SPEED;
        player.facingRight = TRUE;
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_WALK);
        }
    }
    
    // Déplacement vertical avec haut/bas
    if(joy & BUTTON_UP) {
        manualVelY = -PLAYER_SPEED;
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_WALK);
        }
    }
    else if(joy & BUTTON_DOWN) {
        manualVelY = PLAYER_SPEED;
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_WALK);
        }
    }
    
    // Si aucun mouvement, idle
    if(player.velX == 0 && manualVelY == 0) {
        if(player.onGround && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_IDLE);
        }
    }
    
    // Appliquer le déplacement vertical manuel
    if(manualVelY != 0) {
        player.y += manualVelY;
        if(player.y < 0) player.y = 0;
        if(player.y > GROUND_Y) player.y = GROUND_Y;
        // Mettre à jour la position Y de référence
        if(player.onGround) {
            player.groundY = player.y;
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
    
    // Saut avec B
    if((joy & BUTTON_B) && player.onGround) {
        player.velY = PLAYER_JUMP_POWER;
        player.onGround = FALSE;
        if(player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_JUMP);
        }
    }
    
    SPR_setHFlip(player.sprite, !player.facingRight);
}

void updatePlayer() {
    // Déplacement horizontal
    player.x += player.velX;
    
    // Limites de déplacement horizontal
    if(player.x < 0) player.x = 0;
    if(player.x > mapWidth - 16) player.x = mapWidth - 16;
    
    // Gravité et saut
    if(!player.onGround) {
        player.velY += GRAVITY;
        player.y += player.velY;
        
        // Collision avec le sol (retour à la position Y d'origine)
        if(player.y >= player.groundY) {
            player.y = player.groundY;
            player.velY = 0;
            player.onGround = TRUE;
        }
        
        // Animation de chute
        if(player.velY > 0 && player.shootAnimTimer == 0) {
            SPR_setAnim(player.sprite, PLAYERANIM_FALL);
        }
    }
    
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
    
    // Limites de la caméra
    if(targetCameraX < 0) targetCameraX = 0;
    if(targetCameraX > mapWidth - SCREEN_WIDTH) targetCameraX = mapWidth - SCREEN_WIDTH;
    
    cameraX = targetCameraX;
    
    // Mettre à jour la position du sprite du joueur à l'écran
    SPR_setPosition(player.sprite, player.x - cameraX, player.y);
    
    // Mettre à jour les projectiles
    for(u8 i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i].active) {
            SPR_setPosition(bullets[i].sprite, bullets[i].x - cameraX, bullets[i].y);
        }
    }
    
    // Mettre à jour les ennemis
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].active) {
            s16 screenX = enemies[i].x - cameraX;
            if(screenX > -32 && screenX < SCREEN_WIDTH + 32) {
                SPR_setPosition(enemies[i].sprite, screenX, enemies[i].y);
                SPR_setVisibility(enemies[i].sprite, VISIBLE);
            } else {
                SPR_setVisibility(enemies[i].sprite, HIDDEN);
            }
        }
    }
    // Scroll du background à la même vitesse que la caméra
    MAP_scrollTo(bgMap, cameraX, 0);
    
    // Scroll du plan B avec effet parallaxe (moitié de la vitesse)
    if(bgbMap != NULL) {
        MAP_scrollTo(bgbMap, cameraX / 2, 0);
    }
}void checkCollisions() {
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

void spawnEnemy(s16 x, s16 y) {
    for(u8 i = 0; i < MAX_ENEMIES; i++) {
        if(!enemies[i].active) {
            enemies[i].active = TRUE;
            enemies[i].x = x;
            enemies[i].y = y;
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
            enemies[i].hp = ENEMY_MAX_HP;
            enemies[i].maxHp = ENEMY_MAX_HP;
            SPR_setAnim(enemies[i].sprite, EANIM_IDLE);
            break;
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
