/***************************************************************************
                             qgscursors.cpp

                             -------------------
    begin                : 2007
    copyright            : (C) 2007 by Gary E. Sherman
    email                : sherman@mrcc.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgscursors.h"

#define SIP_NO_FILE

// cursors
const char *zoom_in[] =
{
  "16 16 3 1",
  ". c None",
  "a c #000000",
  "# c #ffffff",
  ".....#####......",
  "...##aaaaa##....",
  "..#.a.....a.#...",
  ".#.a...a...a.#..",
  ".#a....a....a#..",
  "#a.....a.....a#.",
  "#a.....a.....a#.",
  "#a.aaaa#aaaa.a#.",
  "#a.....a.....a#.",
  "#a.....a.....a#.",
  ".#a....a....a#..",
  ".#.a...a...aaa#.",
  "..#.a.....a#aaa#",
  "...##aaaaa###aa#",
  ".....#####...###",
  "..............#."
};

const char *zoom_out[] =
{
  "16 16 4 1",
  "b c None",
  ". c None",
  "a c #000000",
  "# c #ffffff",
  ".....#####......",
  "...##aaaaa##....",
  "..#.a.....a.#...",
  ".#.a.......a.#..",
  ".#a.........a#..",
  "#a...........a#.",
  "#a...........a#.",
  "#a.aaaa#aaaa.a#.",
  "#a...........a#.",
  "#a...........a#.",
  ".#a.........a#..",
  ".#.a.......aaa#.",
  "..#.a.....a#aaa#",
  "...##aaaaa###aa#",
  ".....#####...###",
  "..............#."
};


const char *capture_point_cursor[] =
{
  "16 16 3 1",
  " »     c None",
  ".»     c #000000",
  "+»     c #FFFFFF",
  "                ",
  "       +.+      ",
  "      ++.++     ",
  "     +.....+    ",
  "    +.     .+   ",
  "   +.   .   .+  ",
  "  +.    .    .+ ",
  " ++.    .    .++",
  " ... ...+... ...",
  " ++.    .    .++",
  "  +.    .    .+ ",
  "   +.   .   .+  ",
  "   ++.     .+   ",
  "    ++.....+    ",
  "      ++.++     ",
  "       +.+      "
};

const char *select_cursor[] =
{
  "16 16 3 1",
  "# c None",
  "a c #000000",
  ". c #ffffff",
  ".###############",
  "...#############",
  ".aa..###########",
  "#.aaa..a.a.a.a.#",
  "#.aaaaa..#####a#",
  "#a.aaaaaa..###.#",
  "#..aaaaaa...##a#",
  "#a.aaaaa.#####.#",
  "#.#.aaaaa.####a#",
  "#a#.aa.aaa.###.#",
  "#.##..#..aa.##a#",
  "#a##.####.aa.#.#",
  "#.########.aa.a#",
  "#a#########.aa..",
  "#.a.a.a.a.a..a.#",
  "#############.##"
};

const char *identify_cursor[] =
{
  "16 16 3 1",
  "# c None",
  "a c #000000",
  ". c #ffffff",
  ".###########..##",
  "...########.aa.#",
  ".aa..######.aa.#",
  "#.aaa..#####..##",
  "#.aaaaa..##.aa.#",
  "##.aaaaaa...aa.#",
  "##.aaaaaa...aa.#",
  "##.aaaaa.##.aa.#",
  "###.aaaaa.#.aa.#",
  "###.aa.aaa..aa.#",
  "####..#..aa.aa.#",
  "####.####.aa.a.#",
  "##########.aa..#",
  "###########.aa..",
  "############.a.#",
  "#############.##"
};

const char *cross_hair_cursor[] =
{
  "16 16 3 1",
  " »     c None",
  ".»     c #000000",
  "+»     c #FFFFFF",
  "                ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      ",
  "        .       ",
  " +++++     +++++",
  " ......   ......",
  " +++++     +++++",
  "        .       ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      ",
  "       +.+      "
};

const char *sampler_cursor[] =
{
  "16 16 3 1",
  " »     c None",
  ".»     c #000000",
  "+»     c #FFFFFF",
  "..              ",
  ".+.             ",
  " .+..           ",
  "  .++.          ",
  "  .+++.         ",
  "   .+++.        ",
  "    .+++.       ",
  "     .+++...    ",
  "      .++...    ",
  "       ......   ",
  "       .......  ",
  "       ........ ",
  "         .......",
  "          ......",
  "           .....",
  "            ... "
};
