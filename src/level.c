#include <genesis.h>
#include "res/level.h"
#include "res/resources.h"
#include "res/lvl1.h"



Map* bgMap = NULL;
Map* bgbMap = NULL;

void initlevel()
{
    // Charger la palette du background d'abord
    PAL_setPalette(PAL0, palette_lvl.data, CPU);
    PAL_setPalette(3, palette_lvlbg.data, CPU);
    // Charger le tileset pour le plan B (background parallaxe) en premier
    VDP_loadTileSet(&bgb_tileset, TILE_USER_INDEX, DMA);
    
    // Créer et afficher la map sur le plan B (arrière-plan)
    bgbMap = MAP_create(&bgb_map, BG_B, TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, TILE_USER_INDEX));
    
    // Charger le tileset pour le plan A (foreground)
    u16 bgaTileIndex = TILE_USER_INDEX + bgb_tileset.numTile;
    VDP_loadTileSet(&bga_tileset, bgaTileIndex, DMA);
    
    // Créer et afficher la map sur le plan A (avec priorité basse pour voir le plan B)
    bgMap = MAP_create(&bga_map, BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, bgaTileIndex));
    
    // S'assurer que les plans sont visibles
//    VDP_setPlaneSize(64, 32, TRUE);
}