#define CWIDTH 5
#define CHEIGHT 7

/* characters taken from https://datasheet.datasheetarchive.com/originals/scans/Scans-000/Scans-0014620.pdf */

#define EMPTY \
	"     "\
	"     "\
	"     "\
	"     "\
	"     "\
	"     "\
	"     "

char *font[] = {
	/* 00 */
	EMPTY,

	/* 01 */
	EMPTY,

	/* 02 */
	EMPTY,

	/* 03 */
	EMPTY,

	/* 04 */
	EMPTY,

	/* 05 */
	EMPTY,

	/* 06 */
	EMPTY,

	/* 07 */
	EMPTY,

	/* 10 */
	EMPTY,

	/* 11 */
	EMPTY,

	/* 12 */
	EMPTY,

	/* 13 */
	EMPTY,

	/* 14 */
	EMPTY,

	/* 15 */
	EMPTY,

	/* 16 */
	EMPTY,

	/* 17 */
	EMPTY,

	/* 20 */
	EMPTY,

	/* 21 */
	EMPTY,

	/* 22 */
	EMPTY,

	/* 23 */
	EMPTY,

	/* 24 */
	EMPTY,

	/* 25 */
	EMPTY,

	/* 26 */
	EMPTY,

	/* 27 */
	EMPTY,

	/* 30 */
	EMPTY,

	/* 31 */
	EMPTY,

	/* 32 */
	EMPTY,

	/* 33 */
	EMPTY,

	/* 34 */
	EMPTY,

	/* 35 */
	EMPTY,

	/* 36 */
	EMPTY,

	/* 37 */
	EMPTY,

	/* 40 SPACE */
	EMPTY,

	/* 41 ! */
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"     "
	"  *  ",

	/* 42 " */
	" * * "
	" * * "
	" * * "
	"     "
	"     "
	"     "
	"     ",

	/* 43 # */
	" * * "
	" * * "
	"*****"
	" * * "
	"*****"
	" * * "
	" * * ",

	/* 44 $ */
	"  *  "
	" ****"
	"* *  "
	" *** "
	"  * *"
	"**** "
	"  *  ",

	/* 45 % */
	"**  *"
	"**  *"
	"   * "
	"  *  "
	" *   "
	"*  **"
	"*  **",

	/* 46 & */
	"  *  "
	" * * "
	" * * "
	" **  "
	"* * *"
	"*  * "
	" ** *",

	/* 47 ' */
	"  *  "
	" *   "
	"*    "
	"     "
	"     "
	"     "
	"     ",

	/* 50 ( */
	"   * "
	"  *  "
	" *   "
	" *   "
	" *   "
	"  *  "
	"   * ",

	/* 51 ) */
	" *   "
	"  *  "
	"   * "
	"   * "
	"   * "
	"  *  "
	" *   ",

	/* 52 * */
	"     "
	"  *  "
	"* * *"
	" *** "
	"* * *"
	"  *  "
	"     ",

	/* 53 + */
	"     "
	"  *  "
	"  *  "
	"*****"
	"  *  "
	"  *  "
	"     ",

	/* 54 , */
	"     "
	"     "
	"     "
	"     "
	" *   "
	" *   "
	"*    ",

	/* 55 - */
	"     "
	"     "
	"     "
	"*****"
	"     "
	"     "
	"     ",

	/* 56 . */
	"     "
	"     "
	"     "
	"     "
	"     "
	"     "
	"*    ",

	/* 57 / */
	"    *"
	"    *"
	"   * "
	"  *  "
	" *   "
	"*    "
	"*    ",

	/* 60 0 */
	" *** "
	"*   *"
	"*  **"
	"* * *"
	"**  *"
	"*   *"
	" *** ",

	/* 61 1 */
	"  *  "
	" **  "
	"* *  "
	"  *  "
	"  *  "
	"  *  "
	"*****",

	/* 62 2 */
	" *** "
	"*   *"
	"    *"
	"   * "
	" **  "
	"*    "
	"*****",

	/* 63 3 */
	"**** "
	"    *"
	"   * "
	"  *  "
	"   * "
	"    *"
	"**** ",

	/* 64 4 */
	"   * "
	"  ** "
	" * * "
	"*  * "
	"*****"
	"   * "
	"   * ",

	/* 65 5 */
	"*****"
	"*    "
	"**** "
	"    *"
	"    *"
	"*   *"
	" *** ",

	/* 66 6 */
	"  ***"
	" *   "
	"*    "
	"**** "
	"*   *"
	"*   *"
	" *** ",

	/* 67 7 */
	"*****"
	"    *"
	"   * "
	"  *  "
	" *   "
	" *   "
	" *   ",

	/* 70 8 */
	" *** "
	"*   *"
	"*   *"
	" *** "
	"*   *"
	"*   *"
	" *** ",

	/* 71 9 */
	" *** "
	"*   *"
	"*   *"
	" ****"
	"    *"
	"   * "
	"***  ",

	/* 72 : */
	"     "
	"     "
	"  *  "
	"     "
	"     "
	"     "
	"  *  ",

	/* 73 ; */
	"     "
	"     "
	" *   "
	"     "
	" *   "
	" *   "
	"*    ",

	/* 74 < */
	"   **"
	"  *  "
	" *   "
	"*    "
	" *   "
	"  *  "
	"   **",

	/* 75 = */
	"     "
	"     "
	"*****"
	"     "
	"*****"
	"     "
	"     ",

	/* 76 > */
	"**   "
	"  *  "
	"   * "
	"    *"
	"   * "
	"  *  "
	"**   ",

	/* 77 ? */
	" *** "
	"*   *"
	"    *"
	"   * "
	"  *  "
	"     "
	"  *  ",

	/* 100 @ */
	" *** "
	"*   *"
	"* ***"
	"* * *"
	"* ***"
	"*    "
	" *** ",

	/* 101 A */
	" *** "
	"*   *"
	"*   *"
	"*****"
	"*   *"
	"*   *"
	"*   *",

	/* 102 B */
	"**** "
	"*   *"
	"*   *"
	"**** "
	"*   *"
	"*   *"
	"**** ",

	/* 103 C */
	" *** "
	"*   *"
	"*    "
	"*    "
	"*    "
	"*   *"
	" *** ",

	/* 104 D */
	"***  "
	"*  * "
	"*   *"
	"*   *"
	"*   *"
	"*  * "
	"***  ",

	/* 105 E */
	"*****"
	"*    "
	"*    "
	"**** "
	"*    "
	"*    "
	"*****",

	/* 106 F */
	"*****"
	"*    "
	"*    "
	"**** "
	"*    "
	"*    "
	"*    ",

	/* 107 G */
	" *** "
	"*   *"
	"*    "
	"*  **"
	"*   *"
	"*   *"
	" ****",

	/* 110 H */
	"*   *"
	"*   *"
	"*   *"
	"*****"
	"*   *"
	"*   *"
	"*   *",

	/* 111 I */
	" *** "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	" *** ",

	/* 112 J */
	"  ***"
	"   * "
	"   * "
	"   * "
	"*  * "
	"*  * "
	" **  ",

	/* 113 K */
	"*   *"
	"*  * "
	"* *  "
	"**   "
	"* *  "
	"*  * "
	"*   *",

	/* 114 L */
	"*    "
	"*    "
	"*    "
	"*    "
	"*    "
	"*    "
	"*****",

	/* 115 M */
	"*   *"
	"** **"
	"* * *"
	"* * *"
	"*   *"
	"*   *"
	"*   *",

	/* 116 N */
	"*   *"
	"**  *"
	"**  *"
	"* * *"
	"*  **"
	"*  **"
	"*   *",

	/* 117 O */
	" *** "
	"*   *"
	"*   *"
	"*   *"
	"*   *"
	"*   *"
	" *** ",

	/* 120 P */
	"**** "
	"*   *"
	"*   *"
	"**** "
	"*    "
	"*    "
	"*    ",

	/* 121 Q */
	" *** "
	"*   *"
	"*   *"
	"*   *"
	"* *  "
	"*  * "
	" ** *",

	/* 122 R */
	"**** "
	"*   *"
	"*   *"
	"**** "
	"* *  "
	"*  * "
	"*   *",

	/* 123 S */
	" ****"
	"*    "
	"*    "
	" *** "
	"    *"
	"    *"
	"**** ",

	/* 124 T */
	"*****"
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  "
	"  *  ",

	/* 125 U */
	"*   *"
	"*   *"
	"*   *"
	"*   *"
	"*   *"
	"*   *"
	" *** ",

	/* 126 V */
	"*   *"
	"*   *"
	"*   *"
	" * * "
	" * * "
	"  *  "
	"  *  ",

	/* 127 W */
	"*   *"
	"*   *"
	"*   *"
	"* * *"
	"* * *"
	"** **"
	"*   *",

	/* 130 X */
	"*   *"
	"*   *"
	" * * "
	"  *  "
	" * * "
	"*   *"
	"*   *",

	/* 131 Y */
	"*   *"
	"*   *"
	"*   *"
	" *** "
	"  *  "
	"  *  "
	"  *  ",

	/* 132 Z */
	"*****"
	"    *"
	"   * "
	"  *  "
	" *   "
	"*    "
	"*****",

	/* 133 [ */
	"***  "
	"*    "
	"*    "
	"*    "
	"*    "
	"*    "
	"***  ",

	/* 134 \ */
	"*    "
	"*    "
	" *   "
	"  *  "
	"   * "
	"    *"
	"    *",

	/* 135 ] */
	"  ***"
	"    *"
	"    *"
	"    *"
	"    *"
	"    *"
	"  ***",

	/* 136 ^ */
	" *** "
	"*   *"
	"     "
	"     "
	"     "
	"     "
	"     ",

	/* 137 _ */
	"     "
	"     "
	"     "
	"     "
	"     "
	"     "
	"*****",

	/* cursor */
	"*****"
	"*****"
	"*****"
	"*****"
	"*****"
	"*****"
	"*****",

	/* up arrow */
	"  *  "
	" *** "
	"* * *"
	"  *  "
	"  *  "
	"  *  "
	"  *  ",

	/* left arrow */
	"     "
	"  *  "
	" *   "
	"*****"
	" *   "
	"  *  "
	"     ",

};

