// ================================================================
// patterns.h â€” Patterns extraits par midi_pattern_learner.py
//              NE PAS Ã‰DITER â€” regÃ©nÃ©rer avec midi_pattern_learner.py
// ================================================================
#pragma once
#include <cstdint>
#include <array>

namespace midnight_patterns {

// DegrÃ©s scalaires : 0=R, 1=2nd, 2=3rd, 3=4th, 4=5th, 5=6th, 6=7th
// Format identique aux tableaux contours[6][4] du plugin.

// â”€â”€ Contours appris â€” remplacement pour voicesForStyle() â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Remplace: static constexpr int8_t contours[6][4] dans style == 0
static constexpr int8_t learned_contours[6][4] = {
    {  1,  1,  4,  4 },  // rise: 2nd â†’ 2nd â†’ 5th â†’ 5th
    {  1,  5,  4,  4 },  // wave: 2nd â†’ 6th â†’ 5th â†’ 5th
    {  5,  4,  5,  4 },  // wave: 6th â†’ 5th â†’ 6th â†’ 5th
    {  1,  1,  1,  1 },  // rise: 2nd â†’ 2nd â†’ 2nd â†’ 2nd
    {  4,  4,  2,  1 },  // descent: 5th â†’ 5th â†’ 3rd â†’ 2nd
    {  4,  1,  1,  3 },  // valley: 5th â†’ 2nd â†’ 2nd â†’ 4th
};

// â”€â”€ Patterns par source (focus FF / Zelda / Chrono) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

// -- Patterns rythmiques appris ------------------------------------------
// 16 bits par mesure : bit i = 1 si une note demarre sur la i-eme double-croche
// slot 0=temps1  slot 4=temps2  slot 8=temps3  slot 12=temps4
static constexpr uint16_t learned_rhythms[8] = {
    0x5555,  // x.x.x.x.x.x.x.x.  dense (8 onsets)
    0x1155,  // x.x.x.x.x...x...  quarter notes (6 onsets)
    0x007F,  // xxxxxxx.........  light syncopation (7 onsets)
    0x1101,  // x.......x...x...  mixed (3 onsets)
    0x5440,  // ......x...x.x.x.  offbeat (4 onsets)
    0x1111,  // x...x...x...x...  quarter notes (4 onsets)
    0x5F9D,  // x.xxx..xxxxxx.x.  syncopated (11 onsets)
    0x7FFF,  // xxxxxxxxxxxxxxx.  very dense (15 onsets)
};

// â”€â”€ Final Fantasy: Mining Town
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.933 | 6 contours
static constexpr int8_t final_fantasy_mining_town[6][4] = {
    {  5,  0,  4,  3 },  // wave: 6th â†’ R â†’ 5th â†’ 4th  [2Ã—]
    {  4,  1,  3,  5 },  // zigzag-up: 5th â†’ 2nd â†’ 4th â†’ 6th  [2Ã—]
    {  5,  0,  4,  3 },  // wave: 6th â†’ R â†’ 5th â†’ 4th  [2Ã—]
    {  4,  1,  3,  5 },  // zigzag-up: 5th â†’ 2nd â†’ 4th â†’ 6th  [2Ã—]
    {  3,  2,  6,  5 },  // zigzag-up: 4th â†’ 3rd â†’ 7th â†’ 6th  [2Ã—]
    {  6,  3,  1,  5 },  // valley: 7th â†’ 4th â†’ 2nd â†’ 6th  [2Ã—]
};

// â”€â”€ Final Fantasy: One Winged Angel Reborn Arrangement
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.858 | 6 contours
static constexpr int8_t final_fantasy_one_winged_angel_reborn_ar[6][4] = {
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  5,  6,  1,  4 },  // zigzag-down: 6th â†’ 7th â†’ 2nd â†’ 5th  [1Ã—]
    {  1,  6,  1,  4 },  // wave: 2nd â†’ 7th â†’ 2nd â†’ 5th  [1Ã—]
    {  1,  0,  0,  0 },  // descent: 2nd â†’ R â†’ R â†’ R  [21Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
};

// â”€â”€ The Legend of Zelda: Lost Woods
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.847 | 6 contours
static constexpr int8_t the_legend_of_zelda_lost_woods[6][4] = {
    {  2,  1,  2,  1 },  // wave: 3rd â†’ 2nd â†’ 3rd â†’ 2nd  [4Ã—]
    {  2,  4,  6,  2 },  // arch: 3rd â†’ 5th â†’ 7th â†’ 3rd  [2Ã—]
    {  0,  2,  2,  0 },  // arch: R â†’ 3rd â†’ 3rd â†’ R  [2Ã—]
    {  4,  2,  2,  0 },  // descent: 5th â†’ 3rd â†’ 3rd â†’ R  [4Ã—]
    {  2,  1,  2,  1 },  // wave: 3rd â†’ 2nd â†’ 3rd â†’ 2nd  [4Ã—]
    {  2,  4,  6,  2 },  // arch: 3rd â†’ 5th â†’ 7th â†’ 3rd  [2Ã—]
};

// â”€â”€ Chrono: Silent Light
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.844 | 6 contours
static constexpr int8_t chrono_silent_light[6][4] = {
    {  5,  4,  4,  0 },  // descent: 6th â†’ 5th â†’ 5th â†’ R  [6Ã—]
    {  4,  4,  0,  0 },  // descent: 5th â†’ 5th â†’ R â†’ R  [16Ã—]
    {  0,  5,  5,  5 },  // rise: R â†’ 6th â†’ 6th â†’ 6th  [8Ã—]
    {  5,  3,  1,  4 },  // valley: 6th â†’ 4th â†’ 2nd â†’ 5th  [1Ã—]
    {  4,  3,  0,  0 },  // descent: 5th â†’ 4th â†’ R â†’ R  [8Ã—]
    {  2,  0,  0,  2 },  // valley: 3rd â†’ R â†’ R â†’ 3rd  [6Ã—]
};

// â”€â”€ Chrono: 600 AD
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.877 | 6 contours
static constexpr int8_t chrono_600_ad[6][4] = {
    {  0,  1,  2,  4 },  // rise: R â†’ 2nd â†’ 3rd â†’ 5th  [1Ã—]
    {  2,  5,  3,  5 },  // wave: 3rd â†’ 6th â†’ 4th â†’ 6th  [2Ã—]
    {  2,  2,  2,  2 },  // rise: 3rd â†’ 3rd â†’ 3rd â†’ 3rd  [64Ã—]
    {  5,  2,  0,  2 },  // valley: 6th â†’ 3rd â†’ R â†’ 3rd  [3Ã—]
    {  6,  2,  2,  4 },  // valley: 7th â†’ 3rd â†’ 3rd â†’ 5th  [2Ã—]
    {  2,  5,  3,  5 },  // wave: 3rd â†’ 6th â†’ 4th â†’ 6th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Ganondorf
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.855 | 6 contours
static constexpr int8_t the_legend_of_zelda_ganondorf[6][4] = {
    {  6,  6,  6,  0 },  // descent: 7th â†’ 7th â†’ 7th â†’ R  [2Ã—]
    {  0,  2,  2,  3 },  // rise: R â†’ 3rd â†’ 3rd â†’ 4th  [7Ã—]
    {  3,  6,  6,  0 },  // arch: 4th â†’ 7th â†’ 7th â†’ R  [2Ã—]
    {  0,  2,  2,  3 },  // rise: R â†’ 3rd â†’ 3rd â†’ 4th  [7Ã—]
    {  3,  2,  2,  2 },  // descent: 4th â†’ 3rd â†’ 3rd â†’ 3rd  [9Ã—]
    {  2,  4,  4,  3 },  // arch: 3rd â†’ 5th â†’ 5th â†’ 4th  [2Ã—]
};

// â”€â”€ Final Fantasy: Anxious Heart
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.829 | 6 contours
static constexpr int8_t final_fantasy_anxious_heart[6][4] = {
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  2,  0,  0,  0 },  // descent: 3rd â†’ R â†’ R â†’ R  [17Ã—]
    {  2,  0,  0,  1 },  // valley: 3rd â†’ R â†’ R â†’ 2nd  [1Ã—]
    {  3,  0,  0,  0 },  // descent: 4th â†’ R â†’ R â†’ R  [11Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  5,  5,  5 },  // rise: R â†’ 6th â†’ 6th â†’ 6th  [8Ã—]
};

// â”€â”€ Final Fantasy: Costa Del Sol
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.852 | 6 contours
static constexpr int8_t final_fantasy_costa_del_sol[6][4] = {
    {  6,  6,  6,  6 },  // rise: 7th â†’ 7th â†’ 7th â†’ 7th  [23Ã—]
    {  5,  2,  2,  2 },  // descent: 6th â†’ 3rd â†’ 3rd â†’ 3rd  [6Ã—]
    {  6,  6,  6,  6 },  // rise: 7th â†’ 7th â†’ 7th â†’ 7th  [23Ã—]
    {  5,  2,  2,  2 },  // descent: 6th â†’ 3rd â†’ 3rd â†’ 3rd  [6Ã—]
    {  6,  6,  6,  6 },  // rise: 7th â†’ 7th â†’ 7th â†’ 7th  [23Ã—]
    {  5,  2,  2,  0 },  // descent: 6th â†’ 3rd â†’ 3rd â†’ R  [1Ã—]
};

// â”€â”€ Final Fantasy: Jenova Absolute
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.866 | 6 contours
static constexpr int8_t final_fantasy_jenova_absolute[6][4] = {
    {  3,  6,  0,  3 },  // wave: 4th â†’ 7th â†’ R â†’ 4th  [3Ã—]
    {  2,  1,  3,  6 },  // zigzag-up: 3rd â†’ 2nd â†’ 4th â†’ 7th  [3Ã—]
    {  0,  3,  2,  1 },  // wave: R â†’ 4th â†’ 3rd â†’ 2nd  [1Ã—]
    {  3,  6,  0,  3 },  // wave: 4th â†’ 7th â†’ R â†’ 4th  [3Ã—]
    {  2,  1,  3,  6 },  // zigzag-up: 3rd â†’ 2nd â†’ 4th â†’ 7th  [3Ã—]
    {  1,  0,  0,  4 },  // valley: 2nd â†’ R â†’ R â†’ 5th  [4Ã—]
};

// â”€â”€ The Legend of Zelda: Sarias Song
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.867 | 6 contours
static constexpr int8_t the_legend_of_zelda_sarias_song[6][4] = {
    {  2,  1,  4,  1 },  // wave: 3rd â†’ 2nd â†’ 5th â†’ 2nd  [2Ã—]
    {  4,  4,  5,  3 },  // zigzag-down: 5th â†’ 5th â†’ 6th â†’ 4th  [2Ã—]
    {  6,  0,  2,  0 },  // wave: 7th â†’ R â†’ 3rd â†’ R  [4Ã—]
    {  4,  0,  2,  0 },  // wave: 5th â†’ R â†’ 3rd â†’ R  [1Ã—]
    {  3,  1,  4,  1 },  // wave: 4th â†’ 2nd â†’ 5th â†’ 2nd  [1Ã—]
    {  4,  4,  5,  3 },  // zigzag-down: 5th â†’ 5th â†’ 6th â†’ 4th  [2Ã—]
};

// â”€â”€ Final Fantasy: Battle Theme
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.871 | 6 contours
static constexpr int8_t final_fantasy_battle_theme[6][4] = {
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  2,  3,  3,  3 },  // rise: 3rd â†’ 4th â†’ 4th â†’ 4th  [4Ã—]
    {  4,  3,  3,  4 },  // valley: 5th â†’ 4th â†’ 4th â†’ 5th  [6Ã—]
    {  4,  3,  3,  3 },  // descent: 5th â†’ 4th â†’ 4th â†’ 4th  [3Ã—]
    {  1,  5,  6,  2 },  // arch: 2nd â†’ 6th â†’ 7th â†’ 3rd  [1Ã—]
    {  0,  2,  1,  4 },  // wave: R â†’ 3rd â†’ 2nd â†’ 5th  [1Ã—]
};

// â”€â”€ Final Fantasy: Mark of the Traitor
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.890 | 6 contours
static constexpr int8_t final_fantasy_mark_of_the_traitor[6][4] = {
    {  6,  4,  6,  5 },  // wave: 7th â†’ 5th â†’ 7th â†’ 6th  [3Ã—]
    {  6,  4,  6,  5 },  // wave: 7th â†’ 5th â†’ 7th â†’ 6th  [3Ã—]
    {  3,  1,  0,  6 },  // valley: 4th â†’ 2nd â†’ R â†’ 7th  [1Ã—]
    {  5,  0,  0,  5 },  // valley: 6th â†’ R â†’ R â†’ 6th  [4Ã—]
    {  4,  3,  2,  1 },  // descent: 5th â†’ 4th â†’ 3rd â†’ 2nd  [1Ã—]
    {  6,  4,  6,  5 },  // wave: 7th â†’ 5th â†’ 7th â†’ 6th  [3Ã—]
};

// â”€â”€ Final Fantasy: Holding My Thoughts in My Heart
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.834 | 6 contours
static constexpr int8_t final_fantasy_holding_my_thoughts_in_my_[6][4] = {
    {  0,  2,  4,  3 },  // arch: R â†’ 3rd â†’ 5th â†’ 4th  [4Ã—]
    {  2,  2,  0,  0 },  // descent: 3rd â†’ 3rd â†’ R â†’ R  [6Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  2,  2,  4,  3 },  // zigzag-up: 3rd â†’ 3rd â†’ 5th â†’ 4th  [3Ã—]
    {  2,  4,  2,  2 },  // wave: 3rd â†’ 5th â†’ 3rd â†’ 3rd  [3Ã—]
    {  2,  2,  2,  2 },  // rise: 3rd â†’ 3rd â†’ 3rd â†’ 3rd  [64Ã—]
};

// â”€â”€ The Legend of Zelda: Deku Palace
// Instrument: Piano acoustique | OriginalitÃ©: 0.902 | 6 contours
static constexpr int8_t the_legend_of_zelda_deku_palace[6][4] = {
    {  4,  5,  0,  6 },  // wave: 5th â†’ 6th â†’ R â†’ 7th  [2Ã—]
    {  2,  3,  5,  3 },  // arch: 3rd â†’ 4th â†’ 6th â†’ 4th  [2Ã—]
    {  4,  5,  0,  4 },  // wave: 5th â†’ 6th â†’ R â†’ 5th  [3Ã—]
    {  1,  0,  4,  3 },  // zigzag-up: 2nd â†’ R â†’ 5th â†’ 4th  [3Ã—]
    {  4,  5,  0,  6 },  // wave: 5th â†’ 6th â†’ R â†’ 7th  [2Ã—]
    {  2,  3,  5,  3 },  // arch: 3rd â†’ 4th â†’ 6th â†’ 4th  [2Ã—]
};

// â”€â”€ Final Fantasy: Main Theme
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.880 | 6 contours
static constexpr int8_t final_fantasy_main_theme[6][4] = {
    {  0,  0,  1,  2 },  // rise: R â†’ R â†’ 2nd â†’ 3rd  [10Ã—]
    {  1,  1,  0,  5 },  // zigzag-up: 2nd â†’ 2nd â†’ R â†’ 6th  [2Ã—]
    {  2,  0,  1,  2 },  // wave: 3rd â†’ R â†’ 2nd â†’ 3rd  [3Ã—]
    {  2,  1,  5,  5 },  // zigzag-up: 3rd â†’ 2nd â†’ 6th â†’ 6th  [2Ã—]
    {  0,  0,  1,  2 },  // rise: R â†’ R â†’ 2nd â†’ 3rd  [10Ã—]
    {  1,  1,  0,  5 },  // zigzag-up: 2nd â†’ 2nd â†’ R â†’ 6th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Temple of Time
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.828 | 6 contours
static constexpr int8_t the_legend_of_zelda_temple_of_time[6][4] = {
    {  0,  4,  0,  2 },  // wave: R â†’ 5th â†’ R â†’ 3rd  [2Ã—]
    {  2,  4,  0,  2 },  // wave: 3rd â†’ 5th â†’ R â†’ 3rd  [2Ã—]
    {  4,  2,  2,  1 },  // descent: 5th â†’ 3rd â†’ 3rd â†’ 2nd  [2Ã—]
    {  2,  4,  2,  2 },  // wave: 3rd â†’ 5th â†’ 3rd â†’ 3rd  [3Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  4,  4,  0,  2 },  // zigzag-down: 5th â†’ 5th â†’ R â†’ 3rd  [1Ã—]
};

// â”€â”€ The Legend of Zelda: Medley of Time
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.875 | 6 contours
static constexpr int8_t the_legend_of_zelda_medley_of_time[6][4] = {
    {  0,  0,  6,  6 },  // rise: R â†’ R â†’ 7th â†’ 7th  [7Ã—]
    {  0,  0,  0,  2 },  // rise: R â†’ R â†’ R â†’ 3rd  [7Ã—]
    {  1,  1,  1,  1 },  // rise: 2nd â†’ 2nd â†’ 2nd â†’ 2nd  [21Ã—]
    {  0,  2,  4,  3 },  // arch: R â†’ 3rd â†’ 5th â†’ 4th  [4Ã—]
    {  5,  2,  4,  1 },  // wave: 6th â†’ 3rd â†’ 5th â†’ 2nd  [2Ã—]
    {  2,  5,  0,  0 },  // zigzag-down: 3rd â†’ 6th â†’ R â†’ R  [2Ã—]
};

// â”€â”€ Final Fantasy: Descendent of Shinobi
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.861 | 6 contours
static constexpr int8_t final_fantasy_descendent_of_shinobi[6][4] = {
    {  0,  0,  0,  5 },  // rise: R â†’ R â†’ R â†’ 6th  [7Ã—]
    {  0,  6,  0,  5 },  // wave: R â†’ 7th â†’ R â†’ 6th  [1Ã—]
    {  0,  0,  0,  5 },  // rise: R â†’ R â†’ R â†’ 6th  [7Ã—]
    {  0,  6,  6,  0 },  // arch: R â†’ 7th â†’ 7th â†’ R  [5Ã—]
    {  6,  3,  0,  2 },  // valley: 7th â†’ 4th â†’ R â†’ 3rd  [2Ã—]
    {  6,  3,  0,  2 },  // valley: 7th â†’ 4th â†’ R â†’ 3rd  [2Ã—]
};

// â”€â”€ Final Fantasy: Electric de Chocobo
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.898 | 6 contours
static constexpr int8_t final_fantasy_electric_de_chocobo[6][4] = {
    {  4,  2,  5,  3 },  // wave: 5th â†’ 3rd â†’ 6th â†’ 4th  [2Ã—]
    {  2,  5,  4,  1 },  // zigzag-down: 3rd â†’ 6th â†’ 5th â†’ 2nd  [3Ã—]
    {  1,  6,  1,  6 },  // wave: 2nd â†’ 7th â†’ 2nd â†’ 7th  [2Ã—]
    {  0,  3,  0,  5 },  // wave: R â†’ 4th â†’ R â†’ 6th  [2Ã—]
    {  4,  0,  5,  3 },  // wave: 5th â†’ R â†’ 6th â†’ 4th  [4Ã—]
    {  2,  5,  4,  1 },  // zigzag-down: 3rd â†’ 6th â†’ 5th â†’ 2nd  [3Ã—]
};

// â”€â”€ Final Fantasy: Aeriths Theme
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.856 | 6 contours
static constexpr int8_t final_fantasy_aeriths_theme[6][4] = {
    {  4,  0,  0,  0 },  // descent: 5th â†’ R â†’ R â†’ R  [11Ã—]
    {  6,  3,  0,  0 },  // descent: 7th â†’ 4th â†’ R â†’ R  [2Ã—]
    {  4,  0,  1,  0 },  // wave: 5th â†’ R â†’ 2nd â†’ R  [1Ã—]
    {  6,  3,  0,  0 },  // descent: 7th â†’ 4th â†’ R â†’ R  [2Ã—]
    {  4,  3,  0,  3 },  // valley: 5th â†’ 4th â†’ R â†’ 4th  [2Ã—]
    {  5,  0,  0,  4 },  // valley: 6th â†’ R â†’ R â†’ 5th  [2Ã—]
};

// â”€â”€ Final Fantasy: Its Difficult To Stand On Both Feet Isnt It
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.854 | 6 contours
static constexpr int8_t final_fantasy_its_difficult_to_stand_on_[6][4] = {
    {  2,  5,  5,  0 },  // arch: 3rd â†’ 6th â†’ 6th â†’ R  [4Ã—]
    {  4,  0,  0,  6 },  // valley: 5th â†’ R â†’ R â†’ 7th  [3Ã—]
    {  2,  5,  5,  0 },  // arch: 3rd â†’ 6th â†’ 6th â†’ R  [4Ã—]
    {  4,  0,  0,  6 },  // valley: 5th â†’ R â†’ R â†’ 7th  [3Ã—]
    {  2,  5,  6,  5 },  // arch: 3rd â†’ 6th â†’ 7th â†’ 6th  [2Ã—]
    {  1,  4,  5,  4 },  // arch: 2nd â†’ 5th â†’ 6th â†’ 5th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Windmill Hut
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.860 | 6 contours
static constexpr int8_t the_legend_of_zelda_windmill_hut[6][4] = {
    {  2,  0,  0,  0 },  // descent: 3rd â†’ R â†’ R â†’ R  [17Ã—]
    {  0,  0,  1,  2 },  // rise: R â†’ R â†’ 2nd â†’ 3rd  [10Ã—]
    {  2,  2,  6,  4 },  // zigzag-up: 3rd â†’ 3rd â†’ 7th â†’ 5th  [2Ã—]
    {  6,  6,  3,  2 },  // descent: 7th â†’ 7th â†’ 4th â†’ 3rd  [2Ã—]
    {  2,  5,  6,  6 },  // rise: 3rd â†’ 6th â†’ 7th â†’ 7th  [2Ã—]
    {  3,  6,  4,  2 },  // zigzag-down: 4th â†’ 7th â†’ 5th â†’ 3rd  [4Ã—]
};

// â”€â”€ Final Fantasy: One Winged Angel
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.789 | 6 contours
static constexpr int8_t final_fantasy_one_winged_angel[6][4] = {
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  3,  0,  0,  0 },  // descent: 4th â†’ R â†’ R â†’ R  [11Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
};

// â”€â”€ Final Fantasy: Ahead On Our Way Remix
// Instrument: Piano Ã©lectrique 1 | OriginalitÃ©: 0.847 | 6 contours
static constexpr int8_t final_fantasy_ahead_on_our_way_remix[6][4] = {
    {  1,  1,  0,  0 },  // descent: 2nd â†’ 2nd â†’ R â†’ R  [4Ã—]
    {  1,  1,  2,  4 },  // rise: 2nd â†’ 2nd â†’ 3rd â†’ 5th  [4Ã—]
    {  1,  0,  0,  0 },  // descent: 2nd â†’ R â†’ R â†’ R  [21Ã—]
    {  0,  0,  4,  4 },  // rise: R â†’ R â†’ 5th â†’ 5th  [21Ã—]
    {  4,  1,  1,  6 },  // valley: 5th â†’ 2nd â†’ 2nd â†’ 7th  [1Ã—]
    {  4,  4,  4,  6 },  // rise: 5th â†’ 5th â†’ 5th â†’ 7th  [2Ã—]
};

// â”€â”€ Final Fantasy: Gold Saucer
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.897 | 6 contours
static constexpr int8_t final_fantasy_gold_saucer[6][4] = {
    {  4,  0,  2,  4 },  // wave: 5th â†’ R â†’ 3rd â†’ 5th  [6Ã—]
    {  5,  2,  4,  6 },  // zigzag-up: 6th â†’ 3rd â†’ 5th â†’ 7th  [2Ã—]
    {  6,  0,  2,  0 },  // wave: 7th â†’ R â†’ 3rd â†’ R  [4Ã—]
    {  1,  5,  0,  4 },  // wave: 2nd â†’ 6th â†’ R â†’ 5th  [2Ã—]
    {  4,  0,  2,  4 },  // wave: 5th â†’ R â†’ 3rd â†’ 5th  [6Ã—]
    {  5,  2,  3,  4 },  // wave: 6th â†’ 3rd â†’ 4th â†’ 5th  [8Ã—]
};

// â”€â”€ The Legend of Zelda: Kokiri Forest
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.846 | 6 contours
static constexpr int8_t the_legend_of_zelda_kokiri_forest[6][4] = {
    {  3,  0,  0,  0 },  // descent: 4th â†’ R â†’ R â†’ R  [11Ã—]
    {  2,  3,  0,  5 },  // wave: 3rd â†’ 4th â†’ R â†’ 6th  [1Ã—]
    {  3,  3,  2,  2 },  // descent: 4th â†’ 4th â†’ 3rd â†’ 3rd  [5Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  5,  5,  6 },  // rise: R â†’ 6th â†’ 6th â†’ 7th  [1Ã—]
    {  3,  5,  5,  6 },  // rise: 4th â†’ 6th â†’ 6th â†’ 7th  [1Ã—]
};

// â”€â”€ The Legend of Zelda: Gerudo Valley
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.811 | 6 contours
static constexpr int8_t the_legend_of_zelda_gerudo_valley[6][4] = {
    {  6,  3,  5,  0 },  // wave: 7th â†’ 4th â†’ 6th â†’ R  [2Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  2,  2,  2 },  // rise: R â†’ 3rd â†’ 3rd â†’ 3rd  [8Ã—]
    {  2,  4,  4,  4 },  // rise: 3rd â†’ 5th â†’ 5th â†’ 5th  [14Ã—]
    {  0,  2,  2,  2 },  // rise: R â†’ 3rd â†’ 3rd â†’ 3rd  [8Ã—]
    {  0,  2,  2,  2 },  // rise: R â†’ 3rd â†’ 3rd â†’ 3rd  [8Ã—]
};

// â”€â”€ Chrono: Millenial Fair
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.870 | 6 contours
static constexpr int8_t chrono_millenial_fair[6][4] = {
    {  0,  0,  1,  6 },  // rise: R â†’ R â†’ 2nd â†’ 7th  [2Ã—]
    {  5,  1,  3,  3 },  // wave: 6th â†’ 2nd â†’ 4th â†’ 4th  [1Ã—]
    {  4,  5,  5,  0 },  // arch: 5th â†’ 6th â†’ 6th â†’ R  [1Ã—]
    {  2,  4,  0,  0 },  // zigzag-down: 3rd â†’ 5th â†’ R â†’ R  [1Ã—]
    {  1,  5,  0,  0 },  // zigzag-down: 2nd â†’ 6th â†’ R â†’ R  [1Ã—]
    {  4,  4,  3,  2 },  // descent: 5th â†’ 5th â†’ 4th â†’ 3rd  [9Ã—]
};

// â”€â”€ The Legend of Zelda: Song of Storms
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.850 | 6 contours
static constexpr int8_t the_legend_of_zelda_song_of_storms[6][4] = {
    {  4,  4,  4,  4 },  // rise: 5th â†’ 5th â†’ 5th â†’ 5th  [71Ã—]
    {  4,  5,  4,  4 },  // wave: 5th â†’ 6th â†’ 5th â†’ 5th  [6Ã—]
    {  5,  4,  4,  4 },  // descent: 6th â†’ 5th â†’ 5th â†’ 5th  [21Ã—]
    {  4,  4,  4,  4 },  // rise: 5th â†’ 5th â†’ 5th â†’ 5th  [71Ã—]
    {  4,  5,  4,  4 },  // wave: 5th â†’ 6th â†’ 5th â†’ 5th  [6Ã—]
    {  2,  0,  5,  6 },  // zigzag-up: 3rd â†’ R â†’ 6th â†’ 7th  [5Ã—]
};

// â”€â”€ Final Fantasy: Fortress of the Condor
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.818 | 6 contours
static constexpr int8_t final_fantasy_fortress_of_the_condor[6][4] = {
    {  0,  6,  0,  2 },  // wave: R â†’ 7th â†’ R â†’ 3rd  [2Ã—]
    {  0,  6,  0,  6 },  // wave: R â†’ 7th â†’ R â†’ 7th  [4Ã—]
    {  0,  6,  0,  2 },  // wave: R â†’ 7th â†’ R â†’ 3rd  [2Ã—]
    {  0,  6,  0,  6 },  // wave: R â†’ 7th â†’ R â†’ 7th  [4Ã—]
    {  1,  0,  1,  4 },  // zigzag-up: 2nd â†’ R â†’ 2nd â†’ 5th  [1Ã—]
    {  1,  0,  5,  5 },  // zigzag-up: 2nd â†’ R â†’ 6th â†’ 6th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Introduction
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.858 | 6 contours
static constexpr int8_t the_legend_of_zelda_introduction[6][4] = {
    {  5,  2,  5,  5 },  // wave: 6th â†’ 3rd â†’ 6th â†’ 6th  [3Ã—]
    {  1,  5,  2,  2 },  // wave: 2nd â†’ 6th â†’ 3rd â†’ 3rd  [1Ã—]
    {  5,  0,  1,  1 },  // wave: 6th â†’ R â†’ 2nd â†’ 2nd  [4Ã—]
    {  6,  1,  0,  5 },  // valley: 7th â†’ 2nd â†’ R â†’ 6th  [2Ã—]
    {  5,  0,  1,  1 },  // wave: 6th â†’ R â†’ 2nd â†’ 2nd  [4Ã—]
    {  0,  2,  2,  5 },  // rise: R â†’ 3rd â†’ 3rd â†’ 6th  [6Ã—]
};

// â”€â”€ Final Fantasy: Great Warrior
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.856 | 6 contours
static constexpr int8_t final_fantasy_great_warrior[6][4] = {
    {  0,  0,  4,  4 },  // rise: R â†’ R â†’ 5th â†’ 5th  [21Ã—]
    {  3,  0,  0,  0 },  // descent: 4th â†’ R â†’ R â†’ R  [11Ã—]
    {  1,  0,  5,  4 },  // zigzag-up: 2nd â†’ R â†’ 6th â†’ 5th  [4Ã—]
    {  3,  0,  0,  0 },  // descent: 4th â†’ R â†’ R â†’ R  [11Ã—]
    {  2,  0,  4,  4 },  // zigzag-up: 3rd â†’ R â†’ 5th â†’ 5th  [2Ã—]
    {  4,  2,  5,  3 },  // wave: 5th â†’ 3rd â†’ 6th â†’ 4th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Cremias Wagon Ride
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.846 | 6 contours
static constexpr int8_t the_legend_of_zelda_cremias_wagon_ride[6][4] = {
    {  2,  3,  5,  5 },  // rise: 3rd â†’ 4th â†’ 6th â†’ 6th  [2Ã—]
    {  2,  4,  4,  4 },  // rise: 3rd â†’ 5th â†’ 5th â†’ 5th  [14Ã—]
    {  4,  0,  0,  0 },  // descent: 5th â†’ R â†’ R â†’ R  [11Ã—]
    {  2,  4,  4,  5 },  // rise: 3rd â†’ 5th â†’ 5th â†’ 6th  [5Ã—]
    {  3,  2,  2,  2 },  // descent: 4th â†’ 3rd â†’ 3rd â†’ 3rd  [9Ã—]
    {  2,  4,  6,  5 },  // arch: 3rd â†’ 5th â†’ 7th â†’ 6th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Kakariko Village
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.844 | 6 contours
static constexpr int8_t the_legend_of_zelda_kakariko_village[6][4] = {
    {  3,  0,  0,  4 },  // valley: 4th â†’ R â†’ R â†’ 5th  [2Ã—]
    {  4,  4,  3,  3 },  // descent: 5th â†’ 5th â†’ 4th â†’ 4th  [5Ã—]
    {  4,  0,  0,  3 },  // valley: 5th â†’ R â†’ R â†’ 4th  [6Ã—]
    {  0,  0,  0,  4 },  // rise: R â†’ R â†’ R â†’ 5th  [13Ã—]
    {  4,  4,  3,  3 },  // descent: 5th â†’ 5th â†’ 4th â†’ 4th  [5Ã—]
    {  4,  0,  0,  3 },  // valley: 5th â†’ R â†’ R â†’ 4th  [6Ã—]
};

// â”€â”€ Chrono: Chrono Trigger
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.852 | 6 contours
static constexpr int8_t chrono_chrono_trigger[6][4] = {
    {  1,  1,  1,  1 },  // rise: 2nd â†’ 2nd â†’ 2nd â†’ 2nd  [21Ã—]
    {  4,  4,  0,  3 },  // zigzag-down: 5th â†’ 5th â†’ R â†’ 4th  [1Ã—]
    {  1,  0,  0,  6 },  // valley: 2nd â†’ R â†’ R â†’ 7th  [4Ã—]
    {  2,  2,  3,  6 },  // rise: 3rd â†’ 3rd â†’ 4th â†’ 7th  [1Ã—]
    {  4,  4,  4,  4 },  // rise: 5th â†’ 5th â†’ 5th â†’ 5th  [71Ã—]
    {  1,  5,  6,  6 },  // rise: 2nd â†’ 6th â†’ 7th â†’ 7th  [1Ã—]
};

// â”€â”€ Final Fantasy: On That Day 5 Years Ago
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.829 | 6 contours
static constexpr int8_t final_fantasy_on_that_day_5_years_ago[6][4] = {
    {  0,  0,  1,  2 },  // rise: R â†’ R â†’ 2nd â†’ 3rd  [10Ã—]
    {  6,  6,  5,  5 },  // descent: 7th â†’ 7th â†’ 6th â†’ 6th  [5Ã—]
    {  4,  3,  0,  1 },  // valley: 5th â†’ 4th â†’ R â†’ 2nd  [1Ã—]
    {  2,  2,  5,  3 },  // zigzag-up: 3rd â†’ 3rd â†’ 6th â†’ 4th  [1Ã—]
    {  0,  0,  5,  3 },  // zigzag-up: R â†’ R â†’ 6th â†’ 4th  [1Ã—]
    {  3,  3,  0,  5 },  // zigzag-up: 4th â†’ 4th â†’ R â†’ 6th  [2Ã—]
};

// â”€â”€ Final Fantasy: Overworld
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.874 | 6 contours
static constexpr int8_t final_fantasy_overworld[6][4] = {
    {  0,  0,  1,  0 },  // wave: R â†’ R â†’ 2nd â†’ R  [6Ã—]
    {  1,  1,  4,  0 },  // zigzag-down: 2nd â†’ 2nd â†’ 5th â†’ R  [2Ã—]
    {  5,  0,  4,  2 },  // wave: 6th â†’ R â†’ 5th â†’ 3rd  [5Ã—]
    {  6,  6,  4,  2 },  // descent: 7th â†’ 7th â†’ 5th â†’ 3rd  [2Ã—]
    {  5,  2,  2,  1 },  // descent: 6th â†’ 3rd â†’ 3rd â†’ 2nd  [4Ã—]
    {  1,  1,  4,  3 },  // zigzag-up: 2nd â†’ 2nd â†’ 5th â†’ 4th  [1Ã—]
};

// â”€â”€ Final Fantasy: Rufuss Welcoming Ceremony
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.853 | 6 contours
static constexpr int8_t final_fantasy_rufuss_welcoming_ceremony[6][4] = {
    {  0,  0,  0,  3 },  // rise: R â†’ R â†’ R â†’ 4th  [8Ã—]
    {  2,  2,  3,  4 },  // rise: 3rd â†’ 3rd â†’ 4th â†’ 5th  [5Ã—]
    {  6,  4,  3,  0 },  // descent: 7th â†’ 5th â†’ 4th â†’ R  [2Ã—]
    {  2,  3,  4,  2 },  // arch: 3rd â†’ 4th â†’ 5th â†’ 3rd  [2Ã—]
    {  0,  0,  0,  4 },  // rise: R â†’ R â†’ R â†’ 5th  [13Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
};

// â”€â”€ The Legend of Zelda: Stone Tower Temple Inverted
// Instrument: Piano acoustique | OriginalitÃ©: 0.794 | 6 contours
static constexpr int8_t the_legend_of_zelda_stone_tower_temple_i[6][4] = {
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  6 },  // rise: R â†’ R â†’ R â†’ 7th  [11Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
};

// â”€â”€ Final Fantasy: Waltz de Chocobo
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.867 | 6 contours
static constexpr int8_t final_fantasy_waltz_de_chocobo[6][4] = {
    {  5,  4,  0,  1 },  // valley: 6th â†’ 5th â†’ R â†’ 2nd  [1Ã—]
    {  6,  0,  3,  3 },  // wave: 7th â†’ R â†’ 4th â†’ 4th  [2Ã—]
    {  0,  4,  4,  1 },  // arch: R â†’ 5th â†’ 5th â†’ 2nd  [7Ã—]
    {  2,  2,  0,  4 },  // zigzag-up: 3rd â†’ 3rd â†’ R â†’ 5th  [12Ã—]
    {  4,  0,  0,  1 },  // valley: 5th â†’ R â†’ R â†’ 2nd  [7Ã—]
    {  6,  0,  0,  1 },  // valley: 7th â†’ R â†’ R â†’ 2nd  [1Ã—]
};

// â”€â”€ The Legend of Zelda: Clock Town
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.893 | 6 contours
static constexpr int8_t the_legend_of_zelda_clock_town[6][4] = {
    {  0,  5,  2,  6 },  // wave: R â†’ 6th â†’ 3rd â†’ 7th  [4Ã—]
    {  0,  6,  6,  2 },  // arch: R â†’ 7th â†’ 7th â†’ 3rd  [2Ã—]
    {  2,  4,  0,  3 },  // wave: 3rd â†’ 5th â†’ R â†’ 4th  [2Ã—]
    {  0,  5,  2,  6 },  // wave: R â†’ 6th â†’ 3rd â†’ 7th  [4Ã—]
    {  0,  6,  6,  0 },  // arch: R â†’ 7th â†’ 7th â†’ R  [5Ã—]
    {  4,  3,  0,  0 },  // descent: 5th â†’ 4th â†’ R â†’ R  [8Ã—]
};

// â”€â”€ The Legend of Zelda: Hyrule Castle Courtyard
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.835 | 6 contours
static constexpr int8_t the_legend_of_zelda_hyrule_castle_courty[6][4] = {
    {  2,  2,  4,  0 },  // zigzag-down: 3rd â†’ 3rd â†’ 5th â†’ R  [4Ã—]
    {  5,  2,  4,  5 },  // wave: 6th â†’ 3rd â†’ 5th â†’ 6th  [3Ã—]
    {  6,  2,  0,  3 },  // valley: 7th â†’ 3rd â†’ R â†’ 4th  [2Ã—]
    {  5,  2,  0,  1 },  // valley: 6th â†’ 3rd â†’ R â†’ 2nd  [2Ã—]
    {  0,  2,  4,  0 },  // arch: R â†’ 3rd â†’ 5th â†’ R  [2Ã—]
    {  5,  2,  4,  5 },  // wave: 6th â†’ 3rd â†’ 5th â†’ 6th  [3Ã—]
};

// â”€â”€ The Legend of Zelda: The Mayors Office
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.829 | 6 contours
static constexpr int8_t the_legend_of_zelda_the_mayors_office[6][4] = {
    {  0,  0,  1,  0 },  // wave: R â†’ R â†’ 2nd â†’ R  [6Ã—]
    {  2,  0,  6,  3 },  // zigzag-up: 3rd â†’ R â†’ 7th â†’ 4th  [1Ã—]
    {  0,  0,  1,  5 },  // rise: R â†’ R â†’ 2nd â†’ 6th  [1Ã—]
    {  0,  5,  3,  3 },  // wave: R â†’ 6th â†’ 4th â†’ 4th  [1Ã—]
    {  2,  2,  5,  5 },  // rise: 3rd â†’ 3rd â†’ 6th â†’ 6th  [4Ã—]
    {  0,  2,  2,  0 },  // arch: R â†’ 3rd â†’ 3rd â†’ R  [2Ã—]
};

// â”€â”€ Final Fantasy: Cids Theme
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.834 | 6 contours
static constexpr int8_t final_fantasy_cids_theme[6][4] = {
    {  0,  0,  4,  3 },  // zigzag-up: R â†’ R â†’ 5th â†’ 4th  [2Ã—]
    {  4,  2,  2,  4 },  // valley: 5th â†’ 3rd â†’ 3rd â†’ 5th  [5Ã—]
    {  4,  4,  2,  6 },  // zigzag-up: 5th â†’ 5th â†’ 3rd â†’ 7th  [1Ã—]
    {  5,  5,  3,  0 },  // descent: 6th â†’ 6th â†’ 4th â†’ R  [1Ã—]
    {  0,  0,  4,  3 },  // zigzag-up: R â†’ R â†’ 5th â†’ 4th  [2Ã—]
    {  4,  2,  2,  4 },  // valley: 5th â†’ 3rd â†’ 3rd â†’ 5th  [5Ã—]
};

// â”€â”€ The Legend of Zelda: Owls Theme
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.870 | 6 contours
static constexpr int8_t the_legend_of_zelda_owls_theme[6][4] = {
    {  0,  0,  2,  4 },  // rise: R â†’ R â†’ 3rd â†’ 5th  [4Ã—]
    {  5,  5,  6,  6 },  // rise: 6th â†’ 6th â†’ 7th â†’ 7th  [11Ã—]
    {  0,  0,  2,  4 },  // rise: R â†’ R â†’ 3rd â†’ 5th  [4Ã—]
    {  2,  2,  4,  0 },  // zigzag-down: 3rd â†’ 3rd â†’ 5th â†’ R  [4Ã—]
    {  0,  6,  6,  0 },  // arch: R â†’ 7th â†’ 7th â†’ R  [5Ã—]
    {  5,  5,  0,  3 },  // zigzag-down: 6th â†’ 6th â†’ R â†’ 4th  [1Ã—]
};

// â”€â”€ Chrono: Wings That Cross Time
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.818 | 6 contours
static constexpr int8_t chrono_wings_that_cross_time[6][4] = {
    {  2,  2,  2,  1 },  // descent: 3rd â†’ 3rd â†’ 3rd â†’ 2nd  [9Ã—]
    {  2,  2,  2,  2 },  // rise: 3rd â†’ 3rd â†’ 3rd â†’ 3rd  [64Ã—]
    {  0,  0,  3,  0 },  // wave: R â†’ R â†’ 4th â†’ R  [3Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
    {  3,  3,  2,  4 },  // zigzag-up: 4th â†’ 4th â†’ 3rd â†’ 5th  [1Ã—]
    {  3,  3,  3,  0 },  // descent: 4th â†’ 4th â†’ 4th â†’ R  [2Ã—]
};

// â”€â”€ Final Fantasy: A High Wind Takes to the Skies
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.861 | 6 contours
static constexpr int8_t final_fantasy_a_high_wind_takes_to_the_s[6][4] = {
    {  0,  0,  1,  4 },  // rise: R â†’ R â†’ 2nd â†’ 5th  [3Ã—]
    {  4,  4,  2,  1 },  // descent: 5th â†’ 5th â†’ 3rd â†’ 2nd  [3Ã—]
    {  1,  1,  1,  6 },  // rise: 2nd â†’ 2nd â†’ 2nd â†’ 7th  [2Ã—]
    {  6,  6,  6,  2 },  // descent: 7th â†’ 7th â†’ 7th â†’ 3rd  [5Ã—]
    {  4,  4,  4,  0 },  // descent: 5th â†’ 5th â†’ 5th â†’ R  [11Ã—]
    {  0,  6,  5,  4 },  // wave: R â†’ 7th â†’ 6th â†’ 5th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Zeldas Lullaby
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.841 | 6 contours
static constexpr int8_t the_legend_of_zelda_zeldas_lullaby[6][4] = {
    {  6,  6,  1,  1 },  // descent: 7th â†’ 7th â†’ 2nd â†’ 2nd  [3Ã—]
    {  5,  5,  6,  6 },  // rise: 6th â†’ 6th â†’ 7th â†’ 7th  [11Ã—]
    {  1,  1,  5,  5 },  // rise: 2nd â†’ 2nd â†’ 6th â†’ 6th  [3Ã—]
    {  0,  0,  2,  6 },  // rise: R â†’ R â†’ 3rd â†’ 7th  [3Ã—]
    {  6,  6,  6,  3 },  // descent: 7th â†’ 7th â†’ 7th â†’ 4th  [5Ã—]
    {  3,  1,  4,  4 },  // zigzag-up: 4th â†’ 2nd â†’ 5th â†’ 5th  [2Ã—]
};

// â”€â”€ Final Fantasy: Sandy Badlands
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.873 | 6 contours
static constexpr int8_t final_fantasy_sandy_badlands[6][4] = {
    {  2,  6,  6,  1 },  // arch: 3rd â†’ 7th â†’ 7th â†’ 2nd  [2Ã—]
    {  3,  3,  0,  4 },  // zigzag-up: 4th â†’ 4th â†’ R â†’ 5th  [4Ã—]
    {  4,  0,  3,  3 },  // wave: 5th â†’ R â†’ 4th â†’ 4th  [1Ã—]
    {  2,  6,  6,  1 },  // arch: 3rd â†’ 7th â†’ 7th â†’ 2nd  [2Ã—]
    {  3,  3,  0,  4 },  // zigzag-up: 4th â†’ 4th â†’ R â†’ 5th  [4Ã—]
    {  4,  0,  5,  5 },  // zigzag-up: 5th â†’ R â†’ 6th â†’ 6th  [2Ã—]
};

// â”€â”€ The Legend of Zelda: Serenade of Water
// Instrument: Piano Ã  queue | OriginalitÃ©: 0.857 | 6 contours
static constexpr int8_t the_legend_of_zelda_serenade_of_water[6][4] = {
    {  0,  5,  2,  4 },  // wave: R â†’ 6th â†’ 3rd â†’ 5th  [4Ã—]
    {  5,  5,  0,  2 },  // zigzag-down: 6th â†’ 6th â†’ R â†’ 3rd  [1Ã—]
    {  0,  4,  2,  2 },  // wave: R â†’ 5th â†’ 3rd â†’ 3rd  [2Ã—]
    {  4,  2,  0,  3 },  // valley: 5th â†’ 3rd â†’ R â†’ 4th  [1Ã—]
    {  2,  1,  0,  0 },  // descent: 3rd â†’ 2nd â†’ R â†’ R  [3Ã—]
    {  0,  0,  0,  0 },  // rise: R â†’ R â†’ R â†’ R  [104Ã—]
};

} // namespace midnight_patterns
