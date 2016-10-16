#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "smalljac.h"
#include "smalljac_internal.h"

/*
    Copyright (c) 2012 Andrew V. Sutherland
    See LICENSE file for license details.
*/


#define CHECK_START		    16384
#define CHECK_RATIO		    1.5		// tuning parameter, controls how often match is checked
#define REQUIRED_MATCHES     3

#define G1_ZSCALE			2
#define G2_ZSCALE			48

/* 
	There are just 3 genus 1 Sato-tate grous, U(1), N(U(1), and SU(2).  These are distinguished by the signature
	
	    (G1_ZSCALE*dens(a1=0),E[a1^2],E[a1^4])
	    
	(which is overkill, E[a1^4] is already enough to distinguish them)
*/

static struct {
	char *group, *sig, *identity_component, *component_group;
	int components, z1;
	long a1moments[SMALLJAC_ST_MAX_MOMENT+1];	
} g1sigtab[SMALLJAC_G1_ST_GROUPS] = {
{ "U(1)", "[0,2,6]", "U(1)", "C_1", 1, 0, {1, 0, 2, 0, 6, 0, 20, 0, 70, 0, 252, 0, 924, 0, 3432, 0, 12870, 0, 48620, 0, 184756} },
{ "N(U(1))", "[1,1,3]", "U(1)", "C_2", 2, 1, {1, 0, 3, 0, 10, 0, 35, 0, 126, 0, 462, 0, 1716, 0, 6435, 0, 24310, 0, 92378, 0, 352716} },
{ "SU(2)", "[0,1,2]", "SU(2)", "C_1", 1, 0, {1, 0, 1, 0, 2, 0, 5, 0, 14, 0, 42, 0, 132, 0, 429, 0, 1430, 0, 4862, 0, 16796} },
};


/*
	Genus 2 Sato-Tate groups are identified using the notation in FKRS-2012.  We include all 55 groups listed in Table 8, but note that 3 cannot occur.

	Each genus 2 Sato-Tate group in USp(4) is uniquely identified by a signature, which is the 11-tuple:
	
		(G2_ZSCALE*dens(a1=0),G2_ZSCALE*dens(a2=-2,-1,0,1,2),E[a1^2],E[a1^4],E[a2],E[a2^2],E[a2^3])
		
	This is stored as a string in order to make it easier to change in the future (the data it encodes can all be obtained from other elements).
	The "tag" element is deprecated and included purely for historical reasons, it is based on an extension of the a1 distributions originally identified in KS-2009.
*/

static struct {
	char *tag, *group, *sig, *identity_component, *component_group;
	int components, z1, z2[5];
	long a1moments[SMALLJAC_ST_MAX_MOMENT+1], a2moments[SMALLJAC_ST_MAX_MOMENT+1];
} g2sigtab[SMALLJAC_G2_ST_GROUPS] = {
{ "27", "C_1","[0,0,0,0,0,0,8,96,4,18,88]", "U(1)", "C_1", 1, 0, {0,0,0,0,0}, { 1, 0, 8, 0, 96, 0, 1280, 0, 17920, 0, 258048, 0, 3784704, 0, 56229888, 0, 843448320, 0, 12745441280, 0, 193730707456 }, { 1, 4, 18, 88, 454, 2424, 13236, 73392, 411462, 2325976, 13233628, 75682512, 434662684, 2505229744, 14482673832, 83940771168, 487610895942, 2838118247064, 16547996212044, 96635257790352, 565107853947444 }},
{ "13b","C_2","[24,0,0,0,0,0,4,48,2,10,44]", "U(1)", "C_2", 2, 1,  {0,0,0,0,0},{ 1, 0, 4, 0, 48, 0, 640, 0, 8960, 0, 129024, 0, 1892352, 0, 28114944, 0, 421724160, 0, 6372720640, 0, 96865353728 }, { 1, 2, 10, 44, 230, 1212, 6628, 36696, 205766, 1162988, 6616940, 37841256, 217331804, 1252614872, 7241338632, 41970385584, 243805454406, 1419059123532, 8273998130332, 48317628895176, 282553927066100 } },
{ "28", "C_3","[0,0,0,0,0,0,4,36,2,8,34]", "U(1)", "C_3", 3, 0,  {0,0,0,0,0}, { 1, 0, 4, 0, 36, 0, 440, 0, 6020, 0, 86184, 0, 1262184, 0, 18745584, 0, 281158020, 0, 4248512840, 0, 64577025656 }, { 1, 2, 8, 34, 164, 842, 4506, 24726, 137892, 777418, 4417178, 25244606, 144936754, 835218542, 4827968762, 27981448794, 162540429732, 946049505642, 5516028172554, 32211838594686, 188369536235394 }},
{ "29", "C_4","[12,0,0,0,0,0,4,36,2,8,32]", "U(1)", "C_4", 4, 1,  {0,0,0,0,0}, { 1, 0, 4, 0, 36, 0, 400, 0, 5040, 0, 68544, 0, 975744, 0, 14277120, 0, 212509440, 0, 3198807040, 0, 48527271936 }, { 1, 2, 8, 32, 150, 732, 3776, 20064, 109318, 605804, 3400848, 19273344, 110017980, 631507736, 3640727616, 21062751552, 122203267398, 710696364876, 4141536632816, 24176487079488, 141345886797460 }},
{ "30", "C_6","[8,0,0,0,0,0,4,36,2,8,32]", "U(1)", "C_6", 6, 1,  {0,0,0,0,0}, { 1, 0, 4, 0, 36, 0, 400, 0, 4900, 0, 63504, 0, 855624, 0, 11874720, 0, 168725700, 0, 2443252240, 0, 35925065176 }, { 1, 2, 8, 32, 148, 712, 3586, 18524, 97796, 524744, 2854258, 15701644, 87215618, 488476276, 2755532578, 15640987932, 89261369316, 511795517256, 2946440889922, 17023077481868, 98655116249138 }},
{ "21c","D_2","[36,0,0,0,0,0,2,24,1,6,22]", "U(1)", "D_2", 4, 3,  {0,0,0,0,0}, { 1, 0, 2, 0, 24, 0, 320, 0, 4480, 0, 64512, 0, 946176, 0, 14057472, 0, 210862080, 0, 3186360320, 0, 48432676864 }, { 1, 1, 6, 22, 118, 606, 3324, 18348, 102918, 581494, 3308596, 18920628, 108666364, 626307436, 3620671032, 20985192792, 121902733638, 709529561766, 4136999089476, 24158814447588, 141276963625428 }},
{ "12d","D_3","[24,0,0,0,0,0,2,18,1,5,17]", "U(1)", "D_3", 6, 3,  {0,0,0,0,0}, { 1, 0, 2, 0, 18, 0, 220, 0, 3010, 0, 43092, 0, 631092, 0, 9372792, 0, 140579010, 0, 2124256420, 0, 32288512828 }, { 1, 1, 5, 17, 85, 421, 2263, 12363, 68981, 388709, 2208715, 12622303, 72468839, 417609271, 2413986097, 13990724397, 81270221301, 473024752821, 2758014110587, 16105919297343, 94184768210075 }},
{ "17c","D_4","[30,0,0,0,0,0,2,18,1,5,16]", "U(1)", "D_4", 8, 5,  {0,0,0,0,0}, { 1, 0, 2, 0, 18, 0, 200, 0, 2520, 0, 34272, 0, 487872, 0, 7138560, 0, 106254720, 0, 1599403520, 0, 24263635968 }, { 1, 1, 5, 16, 78, 366, 1898, 10032, 54694, 302902, 1700550, 9636672, 55009452, 315753868, 1820365524, 10531375776, 61101640134, 355348182438, 2070768340718, 12088243539744, 70672943491108 }},
{ "15c","D_6","[28,0,0,0,0,0,2,18,1,5,16]", "U(1)", "D_6", 12, 7,  {0,0,0,0,0}, { 1, 0, 2, 0, 18, 0, 200, 0, 2450, 0, 31752, 0, 427812, 0, 5937360, 0, 84362850, 0, 1221626120, 0, 17962532588 }, { 1, 1, 5, 16, 77, 356, 1803, 9262, 48933, 262372, 1427255, 7850822, 43608271, 244238138, 1377768005, 7820493966, 44630691093, 255897758628, 1473220469271, 8511538740934, 49327558216947 } },
{ "31", "T","[12,0,0,0,0,0,2,12,1,4,12]", "U(1)", "A_4", 12, 3,  {0,0,0,0,0}, { 1, 0, 2, 0, 12, 0, 120, 0, 1540, 0, 21672, 0, 316008, 0, 4688112, 0, 70295940, 0, 1062152520, 0, 16144348792 }, { 1, 1, 4, 12, 52, 236, 1202, 6378, 35044, 195924, 1108834, 6323978, 36271314, 208911106, 1207301162, 6996256002, 40637708964, 236519943876, 1379029131698, 8053024147098, 47092572794722 } },
{ "32", "O","[18,0,0,0,0,0,2,12,1,4,11]", "U(1)", "S_4", 24, 9,  {0,0,0,0,0}, { 1, 0, 2, 0, 12, 0, 100, 0, 1050, 0, 12852, 0, 172788, 0, 2453880, 0, 35971650, 0, 537299620, 0, 8119471932 }, { 1, 1, 4, 11, 45, 181, 837, 4047, 20757, 110117, 600669, 3338347, 18811927, 107055703, 613680589, 3536907381, 20469127797, 118843373493, 691783361829, 4035348389499, 23580748075755 }},
{ "13c","J(C_1)","[24,24,0,0,0,0,4,48,1,11,40]", "U(1)", "C_2", 2, 1,  {1,0,0,0,0}, { 1, 0, 4, 0, 48, 0, 640, 0, 8960, 0, 129024, 0, 1892352, 0, 28114944, 0, 421724160, 0, 6372720640, 0, 96865353728 }, { 1, 1, 11, 40, 235, 1196, 6650, 36632, 205859, 1162732, 6617326, 37840232, 217333390, 1252610776, 7241345108, 41970369200, 243805480739, 1419059057996, 8273998237094, 48317628633032, 282553927498010 }},
{ "21b","J(C_2)","[36,12,0,0,0,12,2,24,1,7,22]", "U(1)", "D_2", 4, 3,  {1,0,0,0,1}, { 1, 0, 2, 0, 24, 0, 320, 0, 4480, 0, 64512, 0, 946176, 0, 14057472, 0, 210862080, 0, 3186360320, 0, 48432676864 }, { 1, 1, 7, 22, 123, 606, 3346, 18348, 103011, 581494, 3308982, 18920628, 108667950, 626307436, 3620677508, 20985192792, 121902759971, 709529561766, 4136999196238, 24158814447588, 141276964057338 }},
{ "12c","J(C_3)","[24,8,0,0,16,0,2,18,1,5,16]", "U(1)", "C_6", 6, 3,  {1,0,0,2,0}, { 1, 0, 2, 0, 18, 0, 220, 0, 3010, 0, 43092, 0, 631092, 0, 9372792, 0, 140579010, 0, 2124256420, 0, 32288512828 }, { 1, 1, 5, 16, 85, 416, 2264, 12342, 68989, 388624, 2208760, 12621962, 72469060, 417607906, 2413987112, 13990718936, 81270225789, 473024730976, 2758014129968, 16105919209962, 94184768292460 }},
{ "17b","J(C_4)","[30,6,0,12,0,6,2,18,1,5,16]", "U(1)", "C_4xC_2", 8, 5,  {1,0,2,0,1}, { 1, 0, 2, 0, 18, 0, 200, 0, 2520, 0, 34272, 0, 487872, 0, 7138560, 0, 106254720, 0, 1599403520, 0, 24263635968 }, { 1, 1, 5, 16, 79, 366, 1904, 10032, 54723, 302902, 1700680, 9636672, 55010014, 315753868, 1820367904, 10531375776, 61101650083, 355348182438, 2070768381944, 12088243539744, 70672943660874 }},
{ "15b","J(C_6)","[28,4,8,0,8,4,2,18,1,5,16]", "U(1)", "C_6xC_2", 12, 7,  {1,2,0,2,1}, { 1, 0, 2, 0, 18, 0, 200, 0, 2450, 0, 31752, 0, 427812, 0, 5937360, 0, 84362850, 0, 1221626120, 0, 17962532588 }, { 1, 1, 5, 16, 77, 356, 1804, 9262, 48941, 262372, 1427300, 7850822, 43608492, 244238138, 1377769020, 7820493966, 44630695581, 255897758628, 1473220488652, 8511538740934, 49327558299332 }},
{ "23b","J(D_2)","[42,6,0,0,0,18,1,12,1,5,13]", "U(1)", "D_2xC_2", 8, 7,  {1,0,0,0,3}, { 1, 0, 1, 0, 12, 0, 160, 0, 2240, 0, 32256, 0, 473088, 0, 7028736, 0, 105431040, 0, 1593180160, 0, 24216338432 }, { 1, 1, 5, 13, 67, 311, 1694, 9206, 51587, 290875, 1654810, 9460826, 54335230, 313155766, 1810343708, 10492604588, 60951399587, 354764813651, 2068499675810, 12079407354866, 70638482337002 }},
{ "20", "J(D_3)","[36,4,0,0,8,12,1,9,1,4,10]", "U(1)", "D_6", 12, 9,  {1,0,0,2,3}, { 1, 0, 1, 0, 9, 0, 110, 0, 1505, 0, 21546, 0, 315546, 0, 4686396, 0, 70289505, 0, 1062128210, 0, 16144256414 }, { 1, 1, 4, 10, 48, 216, 1153, 6203, 34576, 194440, 1104699, 6311493, 36235785, 208806001, 1206998510, 6995367660, 40635132496, 236512398256, 1379007142675, 8052959736053, 47092384454563 }},
{ "22", "J(D_4)","[39,3,0,6,0,15,1,9,1,4,10]", "U(1)", "D_4xC_2", 16, 13,  {1,0,2,0,5}, { 1, 0, 1, 0, 9, 0, 100, 0, 1260, 0, 17136, 0, 243936, 0, 3569280, 0, 53127360, 0, 799701760, 0, 12131817984 }, { 1, 1, 4, 10, 45, 191, 973, 5048, 27443, 151579, 850659, 4818848, 27506262, 157878982, 910188906, 5265696080, 30550844643, 177674123987, 1035384268663, 6044121900944, 35336472138770 }},
{ "24", "J(D_6)","[38,2,4,0,4,14,1,9,1,4,10]", "U(1)", "D_6xC_2", 24, 19,  {1,2,0,2,7}, { 1, 0, 1, 0, 9, 0, 100, 0, 1225, 0, 15876, 0, 213906, 0, 2968680, 0, 42181425, 0, 610813060, 0, 8981266294 }, { 1, 1, 4, 10, 44, 186, 923, 4663, 24552, 131314, 713969, 3925923, 21805501, 122121117, 688889464, 3910255175, 22315367392, 127948912082, 736610322017, 4255769501539, 24663779457999 }},
{ "25", "J(T)","[30,2,0,0,16,6,1,6,1,3,7]", "U(1)", "A_4xC_2", 24, 15,  {1,0,0,8,3}, { 1, 0, 1, 0, 6, 0, 60, 0, 770, 0, 10836, 0, 158004, 0, 2344056, 0, 35147970, 0, 531076260, 0, 8072174396 }, { 1, 1, 3, 7, 29, 121, 612, 3200, 17565, 98005, 554588, 3162160, 18136340, 104456236, 603653312, 3498130732, 20318865405, 118259982861, 689514609540, 4026512117240, 23546286572124 }},
{ "26", "J(O)","[33,1,0,6,8,9,1,6,1,3,7]", "U(1)", "S_4xC_2", 48, 33,  {1,0,6,8,9}, { 1, 0, 1, 0, 6, 0, 50, 0, 525, 0, 6426, 0, 86394, 0, 1226940, 0, 17985825, 0, 268649810, 0, 4059735966 }, { 1, 1, 3, 7, 26, 96, 432, 2045, 10432, 55144, 300548, 1669515, 9406817, 53529217, 306843708, 1768459152, 10234577552, 59421708592, 345891735528, 2017674282131, 11790374256331 }},
{ "13", "C_{2,1}","[24,0,0,0,0,24,4,48,3,11,48]", "U(1)", "C_2", 2, 1,  {0,0,0,0,1}, { 1, 0, 4, 0, 48, 0, 640, 0, 8960, 0, 129024, 0, 1892352, 0, 28114944, 0, 421724160, 0, 6372720640, 0, 96865353728 }, { 1, 3, 11, 48, 235, 1228, 6650, 36760, 205859, 1163244, 6617326, 37842280, 217333390, 1252618968, 7241345108, 41970401968, 243805480739, 1419059189068, 8273998237094, 48317629157320, 282553927498010 }},
{ "21d","C_{4,1}","[36,0,0,24,0,0,2,24,1,5,22]", "U(1)", "C_4", 4, 3,  {0,0,2,0,0}, { 1, 0, 2, 0, 24, 0, 320, 0, 4480, 0, 64512, 0, 946176, 0, 14057472, 0, 210862080, 0, 3186360320, 0, 48432676864 }, { 1, 1, 5, 22, 115, 606, 3314, 18348, 102883, 581494, 3308470, 18920628, 108665902, 626307436, 3620669316, 20985192792, 121902727203, 709529561766, 4136999065166, 24158814447588, 141276963533050 }},
{ "12b","C_{6,1}","[24,0,16,0,0,8,2,18,1,5,18]", "U(1)", "C_6", 6, 3,  {0,2,0,0,1},{ 1, 0, 2, 0, 18, 0, 220, 0, 3010, 0, 43092, 0, 631092, 0, 9372792, 0, 140579010, 0, 2124256420, 0, 32288512828 }, { 1, 1, 5, 18, 85, 426, 2264, 12384, 68989, 388794, 2208760, 12622644, 72469060, 417610636, 2413987112, 13990729858, 81270225789, 473024774666, 2758014129968, 16105919384724, 94184768292460 } },
{ "21", "D_{2,1}","[36,0,0,0,0,24,2,24,2,7,26]", "U(1)", "D_2", 4, 3,  {0,0,0,0,2}, { 1, 0, 2, 0, 24, 0, 320, 0, 4480, 0, 64512, 0, 946176, 0, 14057472, 0, 210862080, 0, 3186360320, 0, 48432676864 }, { 1, 2, 7, 26, 123, 622, 3346, 18412, 103011, 581750, 3308982, 18921652, 108667950, 626311532, 3620677508, 20985209176, 121902759971, 709529627302, 4136999196238, 24158814709732, 141276964057338 }},
{ "23", "D_{4,1}","[42,0,0,12,0,12,1,12,1,4,13]", "U(1)", "D_4", 8, 7,  {0,0,2,0,2}, { 1, 0, 1, 0, 12, 0, 160, 0, 2240, 0, 32256, 0, 473088, 0, 7028736, 0, 105431040, 0, 1593180160, 0, 24216338432 }, { 1, 1, 4, 13, 63, 311, 1678, 9206, 51523, 290875, 1654554, 9460826, 54334206, 313155766, 1810339612, 10492604588, 60951383203, 354764813651, 2068499610274, 12079407354866, 70638482074858 }},
{ "20b","D_{6,1}","[36,0,8,0,0,16,1,9,1,4,11]", "U(1)", "D_6", 12, 9,  {0,2,0,0,4}, { 1, 0, 1, 0, 9, 0, 110, 0, 1505, 0, 21546, 0, 315546, 0, 4686396, 0, 70289505, 0, 1062128210, 0, 16144256414 }, { 1, 1, 4, 11, 48, 221, 1153, 6224, 34576, 194525, 1104699, 6311834, 36235785, 208807366, 1206998510, 6995373121, 40635132496, 236512420101, 1379007142675, 8052959823434, 47092384454563 }},
{ "12", "D_{3,2}","[24,0,0,0,0,24,2,18,2,6,21]", "U(1)", "D_3", 6, 3,  {0,0,0,0,3}, { 1, 0, 2, 0, 18, 0, 220, 0, 3010, 0, 43092, 0, 631092, 0, 9372792, 0, 140579010, 0, 2124256420, 0, 32288512828 }, { 1, 2, 6, 21, 90, 437, 2285, 12427, 69074, 388965, 2209101, 12623327, 72470425, 417613367, 2413992573, 13990740781, 81270247634, 473024818357, 2758014217349, 16105919559487, 94184768641985 }},
{ "17", "D_{4,2}","[30,0,0,0,0,24,2,18,2,6,20]", "U(1)", "D_4", 8, 5,  {0,0,0,0,4}, { 1, 0, 2, 0, 18, 0, 200, 0, 2520, 0, 34272, 0, 487872, 0, 7138560, 0, 106254720, 0, 1599403520, 0, 24263635968 }, { 1, 2, 6, 20, 83, 382, 1920, 10096, 54787, 303158, 1700936, 9637696, 55011038, 315757964, 1820372000, 10531392160, 61101666467, 355348247974, 2070768447480, 12088243801888, 70672943923018 }},
{ "15", "D_{6,2}","[28,0,0,0,0,24,2,18,2,6,20]", "U(1)", "D_6", 12, 7,  {0,0,0,0,6}, { 1, 0, 2, 0, 18, 0, 200, 0, 2450, 0, 31752, 0, 427812, 0, 5937360, 0, 84362850, 0, 1221626120, 0, 17962532588 }, { 1, 2, 6, 20, 82, 372, 1825, 9326, 49026, 262628, 1427641, 7851846, 43609857, 244242234, 1377774481, 7820510350, 44630717426, 255897824164, 1473220576033, 8511539003078, 49327558648857 }},
{ "25b","O_1","[30,0,0,12,0,12,1,6,1,3,8]", "U(1)", "S_4", 24, 15,  {0,0,6,0,6}, { 1, 0, 1, 0, 6, 0, 60, 0, 770, 0, 10836, 0, 158004, 0, 2344056, 0, 35147970, 0, 531076260, 0, 8072174396 }, { 1, 1, 3, 8, 30, 126, 617, 3221, 17586, 98090, 554673, 3162501, 18136681, 104457601, 603654677, 3498136193, 20318870866, 118260004706, 689514631385, 4026512204621, 23546286659505 }},
{ "5",  "E_1","[0,0,0,0,0,0,4,32,3,10,37]", "SU(2)", "C_1", 1, 0,  {0,0,0,0,0}, { 1, 0, 4, 0, 32, 0, 320, 0, 3584, 0, 43008, 0, 540672, 0, 7028736, 0, 93716480, 0, 1274544128, 0, 17611882496 }, { 1, 3, 10, 37, 150, 654, 3012, 14445, 71398, 361114, 1859628, 9716194, 51373180, 274352316, 1477635912, 8016865533, 43773564294, 240356635170, 1326359740956, 7351846397334, 40913414754324 }},
{ "11b","E_2","[24,0,0,0,0,0,2,16,1,6,17]", "SU(2)", "C_2", 2, 1,  {0,0,0,0,0}, { 1, 0, 2, 0, 16, 0, 160, 0, 1792, 0, 21504, 0, 270336, 0, 3514368, 0, 46858240, 0, 637272064, 0, 8805941248 }, { 1, 1, 6, 17, 78, 322, 1516, 7205, 35734, 180494, 929940, 4857866, 25687052, 137175300, 738819672, 4008429549, 21886788582, 120178305430, 663179894788, 3675923152478, 20456707469540 }},
{ "4",  "E_3","[0,0,0,0,0,0,2,12,1,4,13]", "SU(2)", "C_3", 3, 0,  {0,0,0,0,0}, { 1, 0, 2, 0, 12, 0, 110, 0, 1204, 0, 14364, 0, 180312, 0, 2343198, 0, 31239780, 0, 424851284, 0, 5870638696 }, { 1, 1, 4, 13, 52, 222, 1014, 4839, 23860, 120526, 620278, 3239788, 17127202, 91458304, 492565662, 2672343909, 14591339748, 80119295718, 442121067510, 2450618669508, 13637813847234 }},
{ "7",  "E_4","[12,0,0,0,0,0,2,12,1,4,11]", "SU(2)", "C_4", 4, 1,  {0,0,0,0,0}, { 1, 0, 2, 0, 12, 0, 100, 0, 1008, 0, 11424, 0, 139392, 0, 1784640, 0, 23612160, 0, 319880704, 0, 4411570176 }, { 1, 1, 4, 11, 46, 182, 824, 3817, 18582, 92678, 473368, 2458326, 12947532, 68959100, 370747056, 2009062197, 10961073126, 60153975110, 331828766744, 1838845207834, 10231635794980 }},
{ "6",  "E_6","[8,0,0,0,0,0,2,12,1,4,11]", "SU(2)", "C_6", 6, 1,  {0,0,0,0,0}, { 1, 0, 2, 0, 12, 0, 100, 0, 980, 0, 10584, 0, 122232, 0, 1484340, 0, 18747300, 0, 244325224, 0, 3265915016 }, { 1, 1, 4, 11, 44, 172, 754, 3397, 16020, 77516, 384578, 1944626, 9997970, 52122566, 275020698, 1466253567, 7888170564, 42773157124, 233548438450, 1283030730346, 7086753445858 }},
{ "11", "J(E_1)","[24,0,0,0,0,0,2,16,2,6,20]", "SU(2)", "C_2", 2, 1,  {0,0,0,0,0}, { 1, 0, 2, 0, 16, 0, 160, 0, 1792, 0, 21504, 0, 270336, 0, 3514368, 0, 46858240, 0, 637272064, 0, 8805941248 }, { 1, 2, 6, 20, 78, 332, 1516, 7240, 35734, 180620, 929940, 4858328, 25687052, 137177016, 738819672, 4008435984, 21886788582, 120178329740, 663179894788, 3675923244856, 20456707469540 }},
{ "18", "J(E_2)","[36,0,0,0,0,0,1,8,1,4,10]", "SU(2)", "D_2", 4, 3,  {0,0,0,0,0}, { 1, 0, 1, 0, 8, 0, 80, 0, 896, 0, 10752, 0, 135168, 0, 1757184, 0, 23429120, 0, 318636032, 0, 4402970624 }, { 1, 1, 4, 10, 42, 166, 768, 3620, 17902, 90310, 465096, 2429164, 12843988, 68588508, 369411552, 2004217992, 10943400726, 60089164870, 331589971704, 1837961622428, 10228353827148 }},
{ "10", "J(E_3)","[24,0,0,0,0,0,1,6,1,3,8]", "SU(2)", "D_3", 6, 3,  {0,0,0,0,0}, { 1, 0, 1, 0, 6, 0, 55, 0, 602, 0, 7182, 0, 90156, 0, 1171599, 0, 15619890, 0, 212425642, 0, 2935319348 }, { 1, 1, 3, 8, 29, 116, 517, 2437, 11965, 60326, 310265, 1620125, 8564063, 45730010, 246284547, 1336175172, 7295676309, 40059660014, 221060558065, 1225309380943, 6818907015995 }},
{ "16", "J(E_4)","[30,0,0,0,0,0,1,6,1,3,7]", "SU(2)", "D_4", 8, 5,  {0,0,0,0,0}, { 1, 0, 1, 0, 6, 0, 50, 0, 504, 0, 5712, 0, 69696, 0, 892320, 0, 11806080, 0, 159940352, 0, 2205785088 }, { 1, 1, 3, 7, 26, 96, 422, 1926, 9326, 46402, 236810, 1229394, 6474228, 34480408, 185375244, 1004534316, 5480542998, 30076999710, 165914407682, 919422650106, 5115817989868 }},
{ "14", "J(E_6)","[28,0,0,0,0,0,1,6,1,3,7]", "SU(2)", "D_6", 12, 7,  {0,0,0,0,0}, { 1, 0, 1, 0, 6, 0, 50, 0, 490, 0, 5292, 0, 61116, 0, 742170, 0, 9373650, 0, 122162612, 0, 1632957508 }, { 1, 1, 3, 7, 25, 91, 387, 1716, 8045, 38821, 192415, 972544, 4999447, 26062141, 137512065, 733130001, 3944091717, 21386590717, 116774243535, 641515411362, 3543376815307 }},
{ "33", "F","[0,0,0,0,0,0,4,36,2,8,32]", "U(1)xU(1)", "C_1", 1, 0,  {0,0,0,0,0}, { 1, 0, 3, 0, 21, 0, 210, 0, 2485, 0, 31878, 0, 427350, 0, 5891028, 0, 82824885, 0, 1181976510, 0, 17067482146 }, { 1, 2, 6, 20, 82, 372, 1824, 9312, 48850, 260804, 1410736, 7708032, 42460840, 235495184, 1313652864, 7364392320, 41464232850, 234349652324, 1328989358544, 7559412595392, 43115245475752 }},
{ "34", "F_a","[0,0,0,0,0,24,3,21,2,6,20]", "U(1)xU(1)", "C_2", 2, 0,  {0,0,0,0,1}, { 1, 0, 3, 0, 21, 0, 210, 0, 2485, 0, 31878, 0, 427350, 0, 5891028, 0, 82824885, 0, 1181976510, 0, 17067482146 }, { 1, 2, 6, 20, 82, 372, 1824, 9312, 48850, 260804, 1410736, 7708032, 42460840, 235495184, 1313652864, 7364392320, 41464232850, 234349652324, 1328989358544, 7559412595392, 43115245475752 }},
{ "35", "F_{ab}","[24,0,0,0,0,24,2,18,2,6,20]", "U(1)xU(1)", "C_2", 2, 1,  {0,0,0,0,1}, { 1, 0, 2, 0, 18, 0, 200, 0, 2450, 0, 31752, 0, 426888, 0, 5889312, 0, 82818450, 0, 1181952200, 0, 17067389768 }, { 1, 2, 6, 20, 82, 372, 1824, 9312, 48850, 260804, 1410736, 7708032, 42460840, 235495184, 1313652864, 7364392320, 41464232850, 234349652324, 1328989358544, 7559412595392, 43115245475752 }},
{ "19", "F_{ac}","[36,0,0,24,0,12,1,9,1,3,10]", "U(1)xU(1)", "C_4", 4, 3,  {0,0,2,0,1}, { 1, 0, 1, 0, 9, 0, 100, 0, 1225, 0, 15876, 0, 213444, 0, 2944656, 0, 41409225, 0, 590976100, 0, 8533694884 }, { 1, 1, 3, 10, 41, 186, 912, 4656, 24425, 130402, 705368, 3854016, 21230420, 117747592, 656826432, 3682196160, 20732116425, 117174826162, 664494679272, 3779706297696, 21557622737876 }},
{ "8",  "F_{a,b}","[12,0,0,0,0,36,2,12,2,5,14]", "U(1)xU(1)", "D_2", 4, 1,  {0,0,0,0,3}, { 6, 0, 1, 0, 9, 0, 100, 0, 1225, 0, 15876, 0, 213444, 0, 2944656, 0, 41409225, 0, 590976100, 0, 8533694884 }, { 1, 1, 3, 7, 26, 101, 477, 2360, 12294, 65329, 353003, 1927520, 10616465, 58875844, 328418170, 1841106272, 10366077814, 58587445849, 332247417327, 1889853279920, 10778811677271 }},
{ "36", "G_{1,3}","[0,0,0,0,0,0,3,20,2,6,20]", "U(1)xSU(2)", "C_1", 1, 0, {0,0,0,0,0}, { 1, 0, 3, 0, 20, 0, 175, 0, 1764, 0, 19404, 0, 226512, 0, 2760615, 0, 34763300, 0, 449141836, 0, 5924217936 }, { 1, 2, 6, 20, 76, 312, 1364, 6232, 29460, 142952, 708328, 3570096, 18251248, 94433120, 493669128, 2603975152, 13843555844, 74109433992, 399194826680, 2162240606992, 11770482003888 }},
{ "3",  "N(G_{1,3})","[0,0,0,0,0,24,2,11,2,5,14]", "U(1)xSU(2)", "C_2", 2, 0, {0,0,0,0,1}, { 1, 0, 2, 0, 11, 0, 90, 0, 889, 0, 9723, 0, 113322, 0, 1380522, 0, 17382365, 0, 224573349, 0, 2962117366 }, { 1, 2, 5, 14, 46, 172, 714, 3180, 14858, 71732, 354676, 1786072, 9127672, 47220656, 246842756, 1302003960, 6921810690, 37054782532, 199597544412, 1081120565640, 5885241526232 }},
{ "2",  "G_{3,3}","[0,0,0,0,0,0,2,10,2,5,14]", "SU(2)xSU(2)", "C_1", 1, 0, {0,0,0,0,0}, { 1, 0, 2, 0, 10, 0, 70, 0, 588, 0, 5544, 0, 56628, 0, 613470, 0, 6952660, 0, 81662152, 0, 987369656 }, { 1, 2, 5, 14, 44, 152, 569, 2270, 9524, 41576, 187348, 866296, 4092400, 19684576, 96156649, 476038222, 2384463044, 12067926920, 61641751124, 317469893176, 1647261806128 }},
{ "9",  "N(G_{3,3})","[24,0,0,0,0,0,1,5,1,3,7]", "SU(2)xSU(2)", "C_2", 2, 1, {0,0,0,0,0}, { 1, 0, 1, 0, 5, 0, 35, 0, 294, 0, 2772, 0, 28314, 0, 306735, 0, 3476330, 0, 40831076, 0, 493684828 }, { 1, 1, 3, 7, 23, 76, 287, 1135, 4769, 20788, 93695, 433148, 2046266, 9842288, 48078539, 238019111, 1192232237, 6033963460, 30820877993, 158734946588, 823630911462 }},
{ "1",  "USp(4)","[0,0,0,0,0,0,1,3,1,2,4]", "USp(4)", "C_1", 1, 0, {0,0,0,0,0}, { 1, 0, 1, 0, 3, 0, 14, 0, 84, 0, 594, 0, 4719, 0, 40898, 0, 379236, 0, 3711916, 0, 37975756 }, { 1, 1, 2, 4, 10, 27, 82, 268, 940, 3476, 13448, 53968, 223412, 949535, 4128594, 18310972, 82645012, 378851428, 1760998280, 8288679056, 39457907128 }},
};

int smalljac_STgroup (char STgroup[16], int genus, int index)
{
	switch ( genus ) {
	case 1:
		if ( index < 0 || index > SMALLJAC_G1_ST_GROUPS ) return 0;
		strcpy (STgroup, g1sigtab[index].group);
		return 1;		
	case 2:
		if ( index < 0 || index > SMALLJAC_G2_ST_GROUPS ) return 0;
		strcpy (STgroup, g2sigtab[index].group);
		return 1;
	default:
		printf ("Unsupported genus in smalljac_STgroup\n");
		return 0;
	}
}

int smalljac_STgroup_info (int genus, char *STgroup, char identity_component[16], char component_group[16], int *components,
					 int z[], long moments[], int n, int m)
{
	register int i;
	
	if ( n > genus ) { printf ("n=%d must be <= genus=%d in smalljac_STgroup_info\n", n, genus); return 0; }
	if ( m < 0 || m > SMALLJAC_ST_MAX_MOMENT+1 ) { printf ("invalid m=%d in smalljac_STgroup_info, must be nonnegative and at most SMALLJAC_MAX_MOMENT+1=%d\n", m, SMALLJAC_ST_MAX_MOMENT+1); return 0; }
	switch ( genus ) {
	case 1:
		for ( i = 0 ; i < SMALLJAC_G1_ST_GROUPS ; i++ ) if ( strcmp (g1sigtab[i].group,STgroup) == 0 ) break;
		if ( i >= SMALLJAC_G1_ST_GROUPS ) return 0;
		if ( identity_component ) strcpy (identity_component, g1sigtab[i].identity_component);
		if ( component_group ) strcpy (component_group, g1sigtab[i].component_group);
		if ( components ) *components = g1sigtab[i].components;
		if ( n == 1) {
			if ( z ) z[0] = g1sigtab[i].z1;
			if ( moments && m > 0 ) { memcpy (moments, g1sigtab[i].a1moments, m*sizeof(moments[0])); }
		}
		return 1;		
	case 2:
		for ( i = 0 ; i < SMALLJAC_G2_ST_GROUPS ; i++ ) if ( strcmp (g2sigtab[i].group,STgroup) == 0 ) break;
		if ( i >= SMALLJAC_G2_ST_GROUPS ) return 0;
		if ( identity_component ) strcpy (identity_component, g2sigtab[i].identity_component);
		if ( component_group ) strcpy (component_group, g2sigtab[i].component_group);
		if ( components ) *components = g2sigtab[i].components;
		if ( n >= 1) {
			if ( z ) z[0] = g2sigtab[i].z1;
			if ( moments && m > 0 ) { memcpy (moments, g2sigtab[i].a1moments, m*sizeof(moments[0])); }
		}
		if ( n >= 2) {
			if ( z ) memcpy (z+1, g2sigtab[i].z2, 5*sizeof(z[0]));
			if ( moments && m > 0 ) { memcpy (moments+m, g2sigtab[i].a2moments, m*sizeof(moments[0])); }
		}
		return 1;
	default:
		printf ("Unsupported genus in smalljac_STgroup_info\n");
		return 0;
	}
}

int smalljac_lookup_g2_STgroup (char STgroup[16], double z1, double z2[5], double m1sq[3], double m2[4])
{
	char sig[64];
	register int i;
	
	sprintf (sig, "[%.f,%.f,%.f,%.f,%.f,%.f,%.f,%.f,%.f,%.f,%.f]", G2_ZSCALE*z1, G2_ZSCALE*z2[0], G2_ZSCALE*z2[1], G2_ZSCALE*z2[2], G2_ZSCALE*z2[3], G2_ZSCALE*z2[4],
		   m1sq[1], m1sq[2], m2[1], m2[2], m2[3]);
	for ( i = 0 ; i < SMALLJAC_G2_ST_GROUPS  ; i++ ) if ( strcmp(sig,g2sigtab[i].sig) == 0 ) break;
	if ( i == SMALLJAC_G2_ST_GROUPS ) return 0;
	strcpy (STgroup, g2sigtab[i].group);
	return 1;
}

int smalljac_lookup_g1_STgroup (char STgroup[16], double z1, double m1sq[3])
{
	char sig[64];
	register int i;
	
	sprintf (sig, "[%.f,%.f,%.f]", G1_ZSCALE*z1, m1sq[1], m1sq[2]);
	for ( i = 0 ; i < SMALLJAC_G1_ST_GROUPS  ; i++ ) if ( strcmp(sig,g1sigtab[i].sig) == 0 ) break;
	if ( i == SMALLJAC_G1_ST_GROUPS ) return 0;
	strcpy (STgroup, g1sigtab[i].group);
	return 1;
}


struct g2_sigtab_ctx {
	int z1cnt;
	int z2cnts[5];
	int cnt;
	int chkp;
	double a1sq[3],a2[4];
	long a1tot;
	int match_count;
	char STgroup[16];
};


int smalljac_identify_STgroup_callback (smalljac_curve_t c, unsigned long p, int good, long a[], int n, void *arg)
{
	struct g2_sigtab_ctx *ctx;
	char STgroup[16];
	double x;
	long i;
    int match;

	if ( n < 1 || n > 2 ) { printf ("Unsupported genus %d in smalljac_identify_STgroup_callback\n", n); return 0; }
	ctx = (struct g2_sigtab_ctx *)arg;
	ctx->cnt++;
	ctx->a1tot += a[0];
	x = (double)(a[0]*a[0])/p;
	ctx->a1sq[1] += x;  ctx->a1sq[2] += x*x;
	if ( ! a[0] ) ctx->z1cnt++;
	if ( n > 1 ) {
		x = (double)a[1]/p;
		ctx->a2[1] += x;  ctx->a2[2] += x*x;  ctx->a2[3] += x*x*x;
		if ( ! (a[1]%(long)p) ) {				// AVS 6/3/2014 fixed bug, was only incrementing z2 counts when a[0]=0, but this need not be the case
			i = a[1] / (long)p;
			if ( i >= -2 && i <= 2 ) ctx->z2cnts[2+i]++;
		}
	}
	if ( p > ctx->chkp ) {
		double z1, z2[5], m1sq[3], m2[4];
		
		z1 = (double)ctx->z1cnt/ctx->cnt;
		m1sq[1] = ctx->a1sq[1]/ctx->cnt; m1sq[2] = ctx->a1sq[2]/ctx->cnt; 
		if ( n > 1 ) {
			m2[1] = ctx->a2[1]/ctx->cnt; m2[2] = ctx->a2[2]/ctx->cnt; m2[3] = ctx->a2[3]/ctx->cnt; 		
			for ( i = 0 ; i < 5 ; i++ ) z2[i] = (double)ctx->z2cnts[i]/ctx->cnt;
			match = smalljac_lookup_g2_STgroup(STgroup, z1, z2, m1sq, m2);
        } else {
            match = smalljac_lookup_g1_STgroup(STgroup, z1, m1sq);
        }
        if ( match ) {
            // don't stop until we see the same group several times in a row
            if ( strcmp (STgroup, ctx->STgroup) == 0) {
                if ( ++ctx->match_count == REQUIRED_MATCHES ) return 0;
            } else {
                strcpy (ctx->STgroup, STgroup); 
                ctx->match_count = 1;
            }
        } else {
            ctx->match_count = 0;
        }
		ctx->chkp = CHECK_RATIO*ctx->chkp;
	}
	return 1;
}

int smalljac_identify_STgroup (smalljac_curve_t c, char STgroup[16], long maxp)
{
	struct g2_sigtab_ctx ctx;
	int err;
	
	if ( smalljac_curve_genus(c) > 2 ) { printf ("unsupported genus %d in smalljac_identify_STgroup\n", smalljac_curve_genus(c)); return 0; }
	if ( ! maxp ) maxp = smalljac_curve_max_p (c);
	memset (&ctx,0,sizeof(ctx));
	ctx.chkp = CHECK_START;
	err =smalljac_Lpolys (c, 7, maxp, SMALLJAC_GOOD_ONLY, smalljac_identify_STgroup_callback, &ctx);		// avoid primes 2,3,5
	if ( err < 0 ) { printf ("smalljac error %d\n", err); return 0; }
	if ( ctx.match_count >= REQUIRED_MATCHES ) { strcpy (STgroup, ctx.STgroup); return 1; }
	return 0;
}
