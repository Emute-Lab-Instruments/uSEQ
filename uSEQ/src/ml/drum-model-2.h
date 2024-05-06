#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

void logic_gate_net(char const *inp, char *out) {
	const char v0 = (char) (inp[5] & ~inp[17]);
	const char v1 = (char) (~(inp[5] ^ inp[0]));
	const char v2 = (char) (~inp[30]);
	const char v3 = (char) (~(inp[16] ^ inp[9]));
	const char v4 = (char) (~(inp[19] | inp[24]));
	const char v5 = (char) (~inp[24] | inp[25]);
	const char v6 = (char) (~inp[29] | inp[14]);
	const char v7 = (char) (~inp[4]);
	const char v8 = (char) (~(inp[25] ^ inp[1]));
	const char v9 = (char) (inp[20] & ~inp[27]);
	const char v10 = (char) (inp[0] & ~inp[17]);
	const char v11 = (char) (~(inp[21] | inp[31]));
	const char v12 = (char) (~inp[26]);
	const char v13 = (char) (~(inp[30] ^ inp[28]));
	const char v14 = (char) (~(inp[19] ^ inp[18]));
	const char v15 = (char) (inp[18] & ~inp[27]);
	const char v16 = (char) (~(inp[15] | inp[3]));
	const char v17 = (char) (~inp[2] | inp[3]);
	const char v18 = (char) (~inp[7]);
	const char v19 = (char) (inp[29]);
	const char v20 = (char) (~(inp[11] ^ inp[10]));
	const char v21 = (char) (~inp[13] | inp[28]);
	const char v22 = (char) (~(inp[26] ^ inp[12]));
	const char v23 = (char) (~inp[15]);
	const char v24 = (char) (~inp[6]);
	const char v25 = (char) (~inp[11] | inp[23]);
	const char v26 = (char) (~(inp[13] | inp[22]));
	const char v27 = (char) (inp[22]);
	const char v28 = (char) (inp[20] | inp[14]);
	const char v29 = (char) (~(inp[16] | inp[2]));
	const char v30 = (char) (~(inp[31] | inp[1]));
	const char v31 = (char) (inp[7]);
	const char v32 = (char) (~v18);
	const char v33 = (char) (~(v8 ^ v9));
	const char v34 = (char) (v10);
	const char v35 = (char) (~v5 | v15);
	const char v36 = (char) (~v2);
	const char v37 = (char) (v24 ^ v11);
	const char v38 = (char) (~v10);
	const char v39 = (char) (~(v12 & v9));
	const char v40 = (char) (~v17 | v13);
	const char v41 = (char) (v30 & v25);
	const char v42 = (char) (v16 & v8);
	const char v43 = (char) (v1 | v17);
	const char v44 = (char) (~(v7 & v23));
	const char v45 = (char) (v6 & ~v2);
	const char v46 = (char) (~v11);
	const char v47 = (char) (~v26);
	const char v48 = (char) (~v16 | v4);
	const char v49 = (char) (~v31);
	const char v50 = (char) (v14 & ~v22);
	const char v51 = (char) (v13);
	const char v52 = (char) (v3 & ~v29);
	const char v53 = (char) (~(v19 | v18));
	const char v54 = (char) (v25 & ~v19);
	const char v55 = (char) (~v1);
	const char v56 = (char) (v20 & v26);
	const char v57 = (char) (v22 & ~v0);
	const char v58 = (char) (v12);
	const char v59 = (char) (v4 & v30);
	const char v60 = (char) (~(v14 & v0));
	const char v61 = (char) (~v27);
	const char v62 = (char) (v5 & ~v15);
	const char v63 = (char) (v28 & v3);
	const char v64 = (char) (~(v55 ^ v45));
	const char v65 = (char) (v34 & ~v42);
	const char v66 = (char) (~(v50 & v59));
	const char v67 = (char) (v38 & ~v44);
	const char v68 = (char) (v57 & ~v32);
	const char v69 = (char) (v37 & ~v40);
	const char v70 = (char) (v34);
	const char v71 = (char) (~v53);
	const char v72 = (char) (v47);
	const char v73 = (char) (~(v57 & v44));
	const char v74 = (char) (~v49);
	const char v75 = (char) (v50 ^ v34);
	const char v76 = (char) (v44 | v36);
	const char v77 = (char) (~v41);
	const char v78 = (char) (~v53);
	const char v79 = (char) (v48 & ~v41);
	const char v80 = (char) (v35);
	const char v81 = (char) (~(v55 ^ v42));
	const char v82 = (char) (v35 | v36);
	const char v83 = (char) (v41 & v44);
	const char v84 = (char) (v62);
	const char v85 = (char) (~(v37 & v53));
	const char v86 = (char) (v34);
	const char v87 = (char) (v42 ^ v54);
	const char v88 = (char) (~v48 | v52);
	const char v89 = (char) (v35 & v36);
	const char v90 = (char) (~v48);
	const char v91 = (char) (v61 & ~v46);
	const char v92 = (char) (v38);
	const char v93 = (char) (v57 | v51);
	const char v94 = (char) (~v38 | v35);
	const char v95 = (char) (v39 ^ v47);
	const char v96 = (char) (v44);
	const char v97 = (char) (v58 ^ v34);
	const char v98 = (char) (v60 & ~v56);
	const char v99 = (char) (~(v47 ^ v58));
	const char v100 = (char) (v41 & v51);
	const char v101 = (char) (v53 & ~v50);
	const char v102 = (char) (~(v52 ^ v55));
	const char v103 = (char) (~v43 | v40);
	const char v104 = (char) (~(v45 ^ v47));
	const char v105 = (char) (~v54 | v49);
	const char v106 = (char) (v43 ^ v61);
	const char v107 = (char) (~(v33 | v53));
	const char v108 = (char) (v41);
	const char v109 = (char) (~(v42 & v63));
	const char v110 = (char) (v37 ^ v45);
	const char v111 = (char) (v54);
	const char v112 = (char) (~(v42 & v33));
	const char v113 = (char) (~v39);
	const char v114 = (char) (~v58 | v56);
	const char v115 = (char) (~v43);
	const char v116 = (char) (v48);
	const char v117 = (char) (~v33);
	const char v118 = (char) (~v51);
	const char v119 = (char) (v46 & ~v59);
	const char v120 = (char) (~v32 | v35);
	const char v121 = (char) (v37);
	const char v122 = (char) (v37 | v63);
	const char v123 = (char) (v55 & ~v54);
	const char v124 = (char) (~(v38 ^ v46));
	const char v125 = (char) (~v50);
	const char v126 = (char) (~v43 | v40);
	const char v127 = (char) (v47 ^ v45);
	const char v128 = (char) (v32);
	const char v129 = (char) (~v40);
	const char v130 = (char) (~(v53 & v32));
	const char v131 = (char) (v46 & v56);
	const char v132 = (char) (v58);
	const char v133 = (char) (~(v41 ^ v54));
	const char v134 = (char) (~(v51 ^ v47));
	const char v135 = (char) (v38);
	const char v136 = (char) (v34 & ~v58);
	const char v137 = (char) (v39 & ~v40);
	const char v138 = (char) (~(v47 ^ v36));
	const char v139 = (char) (v34 ^ v56);
	const char v140 = (char) (~v60);
	const char v141 = (char) (~(v47 ^ v54));
	const char v142 = (char) (~(v61 ^ v59));
	const char v143 = (char) (v44);
	const char v144 = (char) (~v57 | v63);
	const char v145 = (char) (v51 & v61);
	const char v146 = (char) (v36 | v59);
	const char v147 = (char) (v55);
	const char v148 = (char) (v39);
	const char v149 = (char) (~(v53 & v46));
	const char v150 = (char) (~v53);
	const char v151 = (char) (~(v33 | v54));
	const char v152 = (char) (~(v51 & v62));
	const char v153 = (char) (~v59);
	const char v154 = (char) (v58);
	const char v155 = (char) (~v38);
	const char v156 = (char) (v33 ^ v43);
	const char v157 = (char) (~v52 | v60);
	const char v158 = (char) (v54 | v37);
	const char v159 = (char) (v62 & ~v50);
	const char v160 = (char) (v40 & v42);
	const char v161 = (char) (~v41);
	const char v162 = (char) (v58 ^ v50);
	const char v163 = (char) (v49 & ~v57);
	const char v164 = (char) (v35 | v61);
	const char v165 = (char) (~(v39 ^ v60));
	const char v166 = (char) (v45);
	const char v167 = (char) (v56);
	const char v168 = (char) (~(v62 | v46));
	const char v169 = (char) (~v48 | v63);
	const char v170 = (char) (v35 ^ v49);
	const char v171 = (char) (v42);
	const char v172 = (char) (v63);
	const char v173 = (char) (v45 ^ v61);
	const char v174 = (char) (v60);
	const char v175 = (char) (~(v63 ^ v45));
	const char v176 = (char) (~v36);
	const char v177 = (char) (v40);
	const char v178 = (char) (v57 ^ v32);
	const char v179 = (char) (v50 & v38);
	const char v180 = (char) (~(v35 ^ v49));
	const char v181 = (char) (~(v46 | v37));
	const char v182 = (char) (~v63);
	const char v183 = (char) (~v44);
	const char v184 = (char) (v36 & v62);
	const char v185 = (char) (~(v58 ^ v59));
	const char v186 = (char) (v43 & v41);
	const char v187 = (char) (v33 & ~v48);
	const char v188 = (char) (~(v56 | v57));
	const char v189 = (char) (v49 | v45);
	const char v190 = (char) (v62 & v56);
	const char v191 = (char) (~(v52 & v48));
	out[0] = (char) (~v140);
	out[1] = (char) (~v123);
	out[2] = (char) (~v178);
	out[3] = (char) (~v69 | v124);
	out[4] = (char) (v64 ^ v175);
	out[5] = (char) (~v133 | v152);
	out[6] = (char) (~v186);
	out[7] = (char) (v110);
	out[8] = (char) (~v131);
	out[9] = (char) (~v132);
	out[10] = (char) (~(v147 ^ v80));
	out[11] = (char) (v157);
	out[12] = (char) (v89);
	out[13] = (char) (~v169);
	out[14] = (char) (~v173 | v160);
	out[15] = (char) (v136 | v115);
	out[16] = (char) (v144);
	out[17] = (char) (v65 & ~v87);
	out[18] = (char) (~v165);
	out[19] = (char) (~(v142 ^ v164));
	out[20] = (char) (~v182);
	out[21] = (char) (~v125);
	out[22] = (char) (~(v85 | v160));
	out[23] = (char) (v70 & v148);
	out[24] = (char) (v144);
	out[25] = (char) (v155);
	out[26] = (char) (v114);
	out[27] = (char) (v140);
	out[28] = (char) (~v117);
	out[29] = (char) (~v152 | v159);
	out[30] = (char) (v77);
	out[31] = (char) (v185);
	out[32] = (char) (~(v156 ^ v190));
	out[33] = (char) (v111 ^ v92);
	out[34] = (char) (v106);
	out[35] = (char) (v181 ^ v94);
	out[36] = (char) (v161);
	out[37] = (char) (~v123 | v187);
	out[38] = (char) (v126);
	out[39] = (char) (v127);
	out[40] = (char) (~v162);
	out[41] = (char) (~v73 | v170);
	out[42] = (char) (v177 & ~v105);
	out[43] = (char) (v189 ^ v114);
	out[44] = (char) (~(v121 ^ v176));
	out[45] = (char) (v88);
	out[46] = (char) (v112 & ~v141);
	out[47] = (char) (~v176 | v106);
	out[48] = (char) (~v77 | v69);
	out[49] = (char) (~(v110 | v167));
	out[50] = (char) (~v182 | v85);
	out[51] = (char) (~(v93 | v166));
	out[52] = (char) (~v64);
	out[53] = (char) (v135 & v112);
	out[54] = (char) (v95 ^ v134);
	out[55] = (char) (~v93 | v118);
	out[56] = (char) (~v99);
	out[57] = (char) (v180);
	out[58] = (char) (~(v122 & v103));
	out[59] = (char) (~v122);
	out[60] = (char) (~v101 | v117);
	out[61] = (char) (~(v65 | v104));
	out[62] = (char) (~v186);
	out[63] = (char) (~v96);
	out[64] = (char) (v78);
	out[65] = (char) (~v170 | v164);
	out[66] = (char) (~v139);
	out[67] = (char) (v163);
	out[68] = (char) (v163 ^ v95);
	out[69] = (char) (~(v139 ^ v113));
	out[70] = (char) (v78 | v171);
	out[71] = (char) (v73 & ~v157);
	out[72] = (char) (v78 ^ v67);
	out[73] = (char) (v66);
	out[74] = (char) (~(v143 ^ v98));
	out[75] = (char) (~(v132 ^ v146));
	out[76] = (char) (~v71 | v131);
	out[77] = (char) (~(v67 | v142));
	out[78] = (char) (~(v137 ^ v153));
	out[79] = (char) (~(v151 | v107));
	out[80] = (char) (v187 & ~v88);
	out[81] = (char) (v76);
	out[82] = (char) (~(v137 | v187));
	out[83] = (char) (~(v91 & v71));
	out[84] = (char) (v102);
	out[85] = (char) (~(v120 ^ v95));
	out[86] = (char) (v74 & v159);
	out[87] = (char) (v126 | v83);
	out[88] = (char) (v75 & v156);
	out[89] = (char) (v81 & ~v99);
	out[90] = (char) (~v97);
	out[91] = (char) (v76 ^ v155);
	out[92] = (char) (~(v148 ^ v177));
	out[93] = (char) (v183 ^ v136);
	out[94] = (char) (v75 | v82);
	out[95] = (char) (~v94);
	out[96] = (char) (~(v186 ^ v101));
	out[97] = (char) (v116 ^ v105);
	out[98] = (char) (v130 ^ v184);
	out[99] = (char) (~(v94 ^ v135));
	out[100] = (char) (v120 & ~v67);
	out[101] = (char) (~v133 | v149);
	out[102] = (char) (v188 | v72);
	out[103] = (char) (v116 ^ v90);
	out[104] = (char) (v134);
	out[105] = (char) (~(v153 ^ v145));
	out[106] = (char) (v97);
	out[107] = (char) (v168 ^ v106);
	out[108] = (char) (v85 ^ v173);
	out[109] = (char) (~v149 | v167);
	out[110] = (char) (v86);
	out[111] = (char) (~v130 | v183);
	out[112] = (char) (~v79);
	out[113] = (char) (v128 ^ v191);
	out[114] = (char) (v190);
	out[115] = (char) (~(v124 | v128));
	out[116] = (char) (~v129);
	out[117] = (char) (v152 ^ v143);
	out[118] = (char) (~(v150 ^ v163));
	out[119] = (char) (v83);
	out[120] = (char) (v119);
	out[121] = (char) (~v141);
	out[122] = (char) (~v191);
	out[123] = (char) (~v93);
	out[124] = (char) (~v189 | v66);
	out[125] = (char) (v72);
	out[126] = (char) (~v100);
	out[127] = (char) (v190 | v165);
	out[128] = (char) (v158);
	out[129] = (char) (~(v113 & v124));
	out[130] = (char) (~(v181 & v179));
	out[131] = (char) (~(char) 0);
	out[132] = (char) (~(v84 ^ v91));
	out[133] = (char) (v79);
	out[134] = (char) (~v104);
	out[135] = (char) (v92 ^ v82);
	out[136] = (char) (v100);
	out[137] = (char) (~v125);
	out[138] = (char) (~(v121 & v109));
	out[139] = (char) (v168 ^ v127);
	out[140] = (char) (v191);
	out[141] = (char) (v179 & v90);
	out[142] = (char) (v168);
	out[143] = (char) (~v112);
	out[144] = (char) (v84 & v108);
	out[145] = (char) (v172 & ~v158);
	out[146] = (char) (~v118);
	out[147] = (char) (v70 ^ v183);
	out[148] = (char) (~(v82 ^ v145));
	out[149] = (char) (v147 ^ v113);
}

// void apply_logic_gate_net (bool const *inp, int *out, size_t len) {
//     char *inp_temp = malloc(32*sizeof(char));
//     char *out_temp = malloc(150*sizeof(char));
//     char *out_temp_o = malloc(4*sizeof(char));
    
//     for(size_t i = 0; i < len; ++i) {
    
//         // Converting the bool array into a bitpacked array
//         for(size_t d = 0; d < 32; ++d) {
//             char res = (char) 0;
//             for(size_t b = 0; b < 8; ++b) {
//                 res <<= 1;
//                 res += !!(inp[i * 32 * 8 + (8 - b - 1) * 32 + d]);
//             }
//             inp_temp[d] = res;
//         }
    
//         // Applying the logic gate net
//         logic_gate_net(inp_temp, out_temp);
        
//         // GroupSum of the results via logic gate networks
//         for(size_t c = 0; c < 10; ++c) {  // for each class
//             // Initialize the output bits
//             for(size_t d = 0; d < 4; ++d) {
//                 out_temp_o[d] = (char) 0;
//             }
            
//             // Apply the adder logic gate network
//             for(size_t a = 0; a < 15; ++a) {
//                 char carry = out_temp[c * 15 + a];
//                 char out_temp_o_d;
//                 for(int d = 4 - 1; d >= 0; --d) {
//                     out_temp_o_d  = out_temp_o[d];
//                     out_temp_o[d] = carry ^ out_temp_o_d;
//                     carry         = carry & out_temp_o_d;
//                 }
//             }
            
//             // Unpack the result bits
//             for(size_t b = 0; b < 8; ++b) {
//                 const char bit_mask = (char) 1 << b;
//                 int res = 0;
//                 for(size_t d = 0; d < 4; ++d) {
//                     res <<= 1;
//                     res += !!(out_temp_o[d] & bit_mask);
//                 }
//                 out[(i * 8 + b) * 10 + c] = res;
//             }
//         }
//     }
//     free(inp_temp);
//     free(out_temp);
//     free(out_temp_o);
// }

void apply_logic_gate_net_singleval (char const *inp, int *out) {
    char *out_temp = (char*)malloc(150*sizeof(char));
    
    // for(size_t i = 0; i < len; ++i) {
    
        // Applying the logic gate net
        logic_gate_net(inp, out_temp);
				const int classSize = 150/10;

        for(size_t c = 0; c < 10; ++c) {  // for each class
					int classSum = 0;
					for(size_t node=c*classSize; node < (c*classSize) + classSize; node++) {
						classSum += out_temp[node] & 1; //take the lowest bit, ignore the rest	
					}
					out[c] = classSum;
				}
        
    // }
    free(out_temp);
}
