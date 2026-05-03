// ================================================================
// ichigos_patterns.h — extrait par analyze_ichigos.py
//              NE PAS ÉDITER — regenerer avec analyze_ichigos.py
// ================================================================
#pragma once
#include <cstdint>
#include <array>

namespace ichigos_patterns {

// Degres scalaires : 0=R, 1=2nd, 2=3rd, 3=4th, 4=5th, 5=6th, 6=7th

// -- Contours melodie (main droite) -------------------------------------
static constexpr int8_t rh_contours[8][4] = {
    {  4,  3,  1,  1 },  // descent: 5th -> 4th -> 2nd -> 2nd
    {  1,  1,  1,  1 },  // rise: 2nd -> 2nd -> 2nd -> 2nd
    {  2,  1,  5,  4 },  // zigzag-up: 3rd -> 2nd -> 6th -> 5th
    {  4,  3,  2,  4 },  // valley: 5th -> 4th -> 3rd -> 5th
    {  4,  3,  4,  1 },  // wave: 5th -> 4th -> 5th -> 2nd
    {  1,  5,  4,  4 },  // wave: 2nd -> 6th -> 5th -> 5th
    {  5,  5,  5,  5 },  // rise: 6th -> 6th -> 6th -> 6th
    {  1,  1,  1,  5 },  // rise: 2nd -> 2nd -> 2nd -> 6th
};

// -- Rythmes melodie (main droite) --------------------------------------
static constexpr uint16_t rh_rhythms[8] = {
    0x5555,  // x.x.x.x.x.x.x.x.  dense
    0x1111,  // x...x...x...x...  quarter notes
    0x0999,  // x..xx..xx..x....  light syncopation
    0x1001,  // x...........x...  mixed
    0xFFFF,  // xxxxxxxxxxxxxxxx  very dense
    0x0155,  // x.x.x.x.x.......  shuffle
    0x7FFF,  // xxxxxxxxxxxxxxx.  very dense
    0x1501,  // x.......x.x.x...  mixed
};

// -- Rythmes accompagnement (main gauche) -------------------------------
static constexpr uint16_t lh_rhythms[8] = {
    0x1055,  // x.x.x.x.....x...  shuffle
    0x1111,  // x...x...x...x...  quarter notes
    0x7FFF,  // xxxxxxxxxxxxxxx.  very dense
    0x051F,  // xxxxx...x.x.....  shuffle
    0x5555,  // x.x.x.x.x.x.x.x.  dense
    0x7575,  // x.x.xxx.x.x.xxx.  dense
    0x4145,  // x.x...x.x.....x.  offbeat
    0x1511,  // x...x...x.x.x...  quarter notes
};

// -- Voicings accords main gauche (top 12) ------------------------------
// Format : degres simultanes (0=racine, 2=3ce, 4=5te...)
// Compte = nb d'occurrences dans le corpus
// {0, 2}  // R+3rd  (×2650)
// {0, 4}  // R+5th  (×2416)
// {0, 2, 4}  // R+3rd+5th  (×1765)
// {0, 3}  // R+4th  (×1070)
// {0, 5}  // R+6th  (×954)
// {0, 2, 5}  // R+3rd+6th  (×581)
// {0, 3, 5}  // R+4th+6th  (×505)
// {0, 6}  // R+7th  (×492)
// {0, 1}  // R+2nd  (×486)
// {0, 4, 6}  // R+5th+7th  (×332)
// {0, 2, 4, 6}  // R+3rd+5th+7th  (×330)
// {0, 2, 6}  // R+3rd+7th  (×299)

// -- Contours par source ------------------------------------------------

// Chrono Cross
static constexpr int8_t chrono_cross_contours[4][4] = {
    {  1,  5,  0,  2 },  // wave
    {  5,  2,  1,  4 },  // valley
    {  0,  4,  3,  2 },  // wave
    {  4,  6,  0,  1 },  // zigzag-down
};

// Chrono Trigger
static constexpr int8_t chrono_trigger_contours[4][4] = {
    {  5,  6,  2,  3 },  // zigzag-down
    {  0,  5,  1,  4 },  // wave
    {  2,  1,  6,  4 },  // zigzag-up
    {  0,  4,  5,  6 },  // rise
};

// Final Fantasy
static constexpr int8_t final_fantasy_contours[4][4] = {
    {  5,  6,  1,  0 },  // zigzag-down
    {  5,  0,  1,  2 },  // wave
    {  5,  1,  4,  5 },  // wave
    {  0,  5,  6,  5 },  // arch
};

// Final Fantasy Adventure
static constexpr int8_t final_fantasy_adventure_contours[4][4] = {
    {  5,  4,  6,  0 },  // wave
    {  3,  1,  4,  2 },  // wave
    {  1,  0,  6,  4 },  // zigzag-up
    {  1,  0,  2,  4 },  // zigzag-up
};

// Final Fantasy Fables_ Chocobo's Dungeon
static constexpr int8_t final_fantasy_fables_chocobo_s_dungeon_contours[4][4] = {
    {  5,  1,  3,  0 },  // wave
    {  5,  0,  1,  3 },  // wave
    {  1,  0,  4,  3 },  // zigzag-up
    {  4,  3,  0,  1 },  // valley
};

// Final Fantasy I
static constexpr int8_t final_fantasy_i_contours[4][4] = {
    {  6,  5,  4,  1 },  // descent
    {  5,  4,  3,  2 },  // descent
    {  6,  4,  3,  2 },  // descent
    {  1,  0,  5,  4 },  // zigzag-up
};

// Final Fantasy II
static constexpr int8_t final_fantasy_ii_contours[4][4] = {
    {  0,  1,  4,  5 },  // rise
    {  4,  2,  3,  5 },  // zigzag-up
    {  0,  2,  4,  5 },  // rise
    {  5,  3,  3,  4 },  // valley
};

// Final Fantasy III
static constexpr int8_t final_fantasy_iii_contours[4][4] = {
    {  1,  5,  2,  0 },  // zigzag-down
    {  6,  4,  1,  3 },  // valley
    {  0,  5,  2,  3 },  // wave
    {  1,  3,  0,  5 },  // wave
};

// Final Fantasy IV
static constexpr int8_t final_fantasy_iv_contours[4][4] = {
    {  4,  5,  6,  0 },  // arch
    {  5,  4,  2,  1 },  // descent
    {  0,  6,  4,  3 },  // wave
    {  3,  0,  5,  6 },  // zigzag-up
};

// Final Fantasy IX
static constexpr int8_t final_fantasy_ix_contours[4][4] = {
    {  4,  5,  6,  3 },  // arch
    {  5,  1,  4,  2 },  // wave
    {  4,  5,  6,  2 },  // arch
    {  4,  5,  3,  2 },  // zigzag-down
};

// Final Fantasy V
static constexpr int8_t final_fantasy_v_contours[4][4] = {
    {  6,  4,  2,  5 },  // valley
    {  1,  0,  4,  5 },  // zigzag-up
    {  1,  2,  6,  0 },  // arch
    {  0,  5,  2,  4 },  // wave
};

// Final Fantasy VI
static constexpr int8_t final_fantasy_vi_contours[4][4] = {
    {  1,  4,  6,  5 },  // arch
    {  1,  6,  5,  4 },  // wave
    {  0,  1,  5,  4 },  // arch
    {  6,  4,  2,  5 },  // valley
};

// Final Fantasy VII
static constexpr int8_t final_fantasy_vii_contours[4][4] = {
    {  4,  1,  6,  3 },  // wave
    {  5,  3,  6,  1 },  // wave
    {  4,  1,  2,  3 },  // wave
    {  4,  5,  6,  2 },  // arch
};

// Final Fantasy VII-Crisis Core
static constexpr int8_t final_fantasy_vii_crisis_core_contours[4][4] = {
    {  6,  1,  5,  4 },  // wave
    {  1,  3,  5,  2 },  // arch
    {  4,  1,  5,  2 },  // wave
    {  1,  2,  3,  5 },  // rise
};

// Final Fantasy VIII
static constexpr int8_t final_fantasy_viii_contours[4][4] = {
    {  5,  1,  2,  3 },  // wave
    {  1,  4,  5,  6 },  // rise
    {  3,  5,  6,  1 },  // arch
    {  1,  6,  4,  5 },  // wave
};

// Final Fantasy VII_ AC
static constexpr int8_t final_fantasy_vii_ac_contours[4][4] = {
    {  5,  0,  3,  2 },  // wave
    {  2,  4,  5,  3 },  // arch
    {  0,  6,  2,  3 },  // wave
    {  5,  0,  1,  2 },  // wave
};

// Final Fantasy X
static constexpr int8_t final_fantasy_x_contours[4][4] = {
    {  6,  5,  4,  3 },  // descent
    {  5,  1,  2,  4 },  // wave
    {  6,  4,  5,  2 },  // wave
    {  0,  1,  2,  3 },  // rise
};

// Final Fantasy X-2
static constexpr int8_t final_fantasy_x_2_contours[4][4] = {
    {  6,  3,  2,  5 },  // valley
    {  4,  5,  6,  2 },  // arch
    {  3,  6,  2,  5 },  // wave
    {  4,  0,  3,  5 },  // zigzag-up
};

// Final Fantasy XI
static constexpr int8_t final_fantasy_xi_contours[4][4] = {
    {  5,  4,  3,  2 },  // descent
    {  2,  5,  3,  4 },  // wave
    {  0,  6,  4,  3 },  // wave
    {  4,  2,  5,  0 },  // wave
};

// Final Fantasy XII
static constexpr int8_t final_fantasy_xii_contours[4][4] = {
    {  4,  5,  6,  2 },  // arch
    {  4,  6,  3,  1 },  // zigzag-down
    {  3,  4,  5,  2 },  // arch
    {  2,  5,  0,  1 },  // zigzag-down
};

// Final Fantasy XIII
static constexpr int8_t final_fantasy_xiii_contours[4][4] = {
    {  5,  1,  0,  3 },  // valley
    {  6,  5,  3,  0 },  // descent
    {  1,  4,  5,  6 },  // rise
    {  1,  4,  6,  2 },  // arch
};

// Final Fantasy XIII-2
static constexpr int8_t final_fantasy_xiii_2_contours[4][4] = {
    {  5,  4,  3,  1 },  // descent
    {  5,  0,  1,  4 },  // wave
    {  5,  5,  4,  3 },  // descent
    {  4,  5,  5,  0 },  // arch
};

// Final Fantasy XIV
static constexpr int8_t final_fantasy_xiv_contours[1][4] = {
    {  0,  0,  0,  2 },  // rise
};

// Final Fantasy XV
static constexpr int8_t final_fantasy_xv_contours[4][4] = {
    {  1,  5,  2,  3 },  // wave
    {  1,  6,  5,  3 },  // wave
    {  6,  1,  2,  3 },  // wave
    {  1,  5,  6,  4 },  // arch
};

// Kingdom Hearts
static constexpr int8_t kingdom_hearts_contours[4][4] = {
    {  1,  5,  6,  3 },  // arch
    {  5,  1,  2,  3 },  // wave
    {  6,  1,  2,  5 },  // wave
    {  6,  1,  5,  0 },  // wave
};

// Kingdom Hearts 2
static constexpr int8_t kingdom_hearts_2_contours[4][4] = {
    {  4,  1,  5,  2 },  // wave
    {  1,  0,  6,  5 },  // zigzag-up
    {  3,  5,  1,  4 },  // wave
    {  2,  4,  1,  5 },  // wave
};

// Kingdom Hearts 358_2 Days
static constexpr int8_t kingdom_hearts_358_2_days_contours[4][4] = {
    {  1,  6,  5,  2 },  // wave
    {  6,  4,  3,  0 },  // descent
    {  5,  4,  0,  3 },  // valley
    {  2,  6,  5,  1 },  // zigzag-down
};

// Kingdom Hearts II
static constexpr int8_t kingdom_hearts_ii_contours[4][4] = {
    {  5,  1,  2,  3 },  // wave
    {  1,  6,  3,  5 },  // wave
    {  4,  1,  5,  2 },  // wave
    {  4,  5,  6,  1 },  // arch
};

// Kingdom Hearts III
static constexpr int8_t kingdom_hearts_iii_contours[4][4] = {
    {  6,  4,  3,  5 },  // valley
    {  1,  0,  6,  5 },  // zigzag-up
    {  4,  0,  6,  5 },  // zigzag-up
    {  1,  4,  2,  0 },  // zigzag-down
};

// Kingdom Hearts II_ Final Mix
static constexpr int8_t kingdom_hearts_ii_final_mix_contours[4][4] = {
    {  6,  2,  5,  3 },  // wave
    {  3,  1,  5,  4 },  // zigzag-up
    {  4,  5,  2,  0 },  // zigzag-down
    {  5,  0,  4,  2 },  // wave
};

// Kingdom Hearts_ 358_2 Days
static constexpr int8_t kingdom_hearts_358_2_days_contours[4][4] = {
    {  6,  4,  3,  0 },  // descent
    {  5,  4,  0,  3 },  // valley
    {  1,  2,  4,  6 },  // rise
    {  0,  2,  4,  6 },  // rise
};

// Kingdom Hearts_ Birth By Sleep
static constexpr int8_t kingdom_hearts_birth_by_sleep_contours[4][4] = {
    {  6,  5,  2,  0 },  // descent
    {  2,  1,  0,  3 },  // valley
    {  2,  3,  4,  5 },  // rise
    {  5,  0,  4,  6 },  // zigzag-up
};

// Kingdom Hearts_ Chain of Memories
static constexpr int8_t kingdom_hearts_chain_of_memories_contours[4][4] = {
    {  3,  1,  5,  2 },  // wave
    {  1,  4,  2,  3 },  // wave
    {  6,  4,  2,  5 },  // valley
    {  5,  2,  3,  1 },  // wave
};

// Legend of Zelda
static constexpr int8_t legend_of_zelda_contours[4][4] = {
    {  5,  4,  3,  1 },  // descent
    {  5,  4,  3,  2 },  // descent
    {  1,  3,  4,  2 },  // arch
    {  1,  5,  5,  6 },  // rise
};

// The Legend of Zelda
static constexpr int8_t the_legend_of_zelda_contours[4][4] = {
    {  4,  0,  5,  2 },  // wave
    {  6,  2,  1,  4 },  // valley
    {  5,  2,  0,  6 },  // valley
    {  4,  2,  1,  0 },  // descent
};

// The Legend of Zelda 25th Anniversary Concert CD
static constexpr int8_t the_legend_of_zelda_25th_anniversary_con_contours[4][4] = {
    {  4,  5,  6,  0 },  // arch
    {  3,  0,  6,  5 },  // zigzag-up
    {  4,  0,  1,  2 },  // wave
    {  0,  2,  4,  6 },  // rise
};

// Zelda
static constexpr int8_t zelda_contours[4][4] = {
    {  4,  0,  5,  2 },  // wave
    {  6,  2,  1,  4 },  // valley
    {  5,  2,  0,  6 },  // valley
    {  4,  2,  1,  0 },  // descent
};

// Zelda - A Link to the Past Between Worlds
static constexpr int8_t zelda_a_link_to_the_past_between_worlds_contours[4][4] = {
    {  4,  1,  1,  3 },  // valley
    {  2,  4,  4,  5 },  // rise
    {  5,  1,  5,  5 },  // wave
    {  1,  1,  1,  3 },  // rise
};

// Zelda - Breath of the Wild
static constexpr int8_t zelda_breath_of_the_wild_contours[4][4] = {
    {  6,  5,  4,  3 },  // descent
    {  4,  6,  5,  5 },  // wave
    {  6,  6,  5,  5 },  // descent
    {  3,  3,  2,  2 },  // descent
};

// Zelda - Legend of Zelda
static constexpr int8_t zelda_legend_of_zelda_contours[4][4] = {
    {  4,  1,  5,  2 },  // wave
    {  4,  3,  5,  6 },  // zigzag-up
    {  6,  1,  0,  2 },  // valley
    {  0,  5,  3,  6 },  // wave
};

// Zelda - Link to the Past
static constexpr int8_t zelda_link_to_the_past_contours[4][4] = {
    {  0,  1,  3,  5 },  // rise
    {  6,  5,  1,  5 },  // valley
    {  6,  5,  5,  4 },  // descent
    {  1,  1,  4,  6 },  // rise
};

// Zelda - Link's Awakening
static constexpr int8_t zelda_link_s_awakening_contours[4][4] = {
    {  4,  3,  2,  5 },  // valley
    {  0,  3,  5,  6 },  // rise
    {  0,  1,  2,  4 },  // rise
    {  2,  3,  5,  6 },  // rise
};

// Zelda - Majora's Mask
static constexpr int8_t zelda_majora_s_mask_contours[4][4] = {
    {  3,  1,  2,  0 },  // wave
    {  2,  5,  2,  3 },  // wave
    {  6,  6,  0,  5 },  // zigzag-down
    {  6,  6,  0,  3 },  // zigzag-down
};

// Zelda - Ocarina of Time
static constexpr int8_t zelda_ocarina_of_time_contours[4][4] = {
    {  6,  5,  3,  2 },  // descent
    {  6,  5,  1,  2 },  // valley
    {  3,  1,  5,  2 },  // wave
    {  5,  3,  6,  4 },  // wave
};

// Zelda - Skyward Sword
static constexpr int8_t zelda_skyward_sword_contours[4][4] = {
    {  1,  3,  6,  0 },  // arch
    {  6,  1,  0,  4 },  // valley
    {  4,  1,  0,  2 },  // valley
    {  0,  5,  4,  6 },  // wave
};

// Zelda - Spirit Tracks
static constexpr int8_t zelda_spirit_tracks_contours[4][4] = {
    {  1,  6,  5,  3 },  // wave
    {  4,  2,  0,  6 },  // valley
    {  5,  5,  6,  4 },  // zigzag-down
    {  4,  6,  5,  5 },  // wave
};

// Zelda - Twilight Princess
static constexpr int8_t zelda_twilight_princess_contours[4][4] = {
    {  6,  1,  5,  0 },  // wave
    {  6,  5,  4,  3 },  // descent
    {  5,  4,  6,  3 },  // wave
    {  4,  1,  5,  0 },  // wave
};

// Zelda - the Wind Waker
static constexpr int8_t zelda_the_wind_waker_contours[4][4] = {
    {  4,  1,  6,  5 },  // zigzag-up
    {  6,  1,  4,  2 },  // wave
    {  0,  6,  5,  3 },  // wave
    {  1,  2,  4,  3 },  // arch
};

// Zelda II_The Adventure of Link
static constexpr int8_t zelda_ii_the_adventure_of_link_contours[4][4] = {
    {  5,  3,  1,  0 },  // descent
    {  5,  2,  6,  0 },  // wave
    {  2,  5,  5,  1 },  // arch
    {  5,  6,  6,  0 },  // arch
};

// Zelda- Skyward Sword
static constexpr int8_t zelda_skyward_sword_contours[4][4] = {
    {  2,  1,  3,  5 },  // zigzag-up
    {  1,  4,  2,  2 },  // wave
    {  6,  5,  4,  4 },  // descent
    {  4,  3,  1,  1 },  // descent
};


} // namespace ichigos_patterns
