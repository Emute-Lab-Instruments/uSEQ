#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

void logic_gate_net(char const *inp, char *out) {
	const char v0 = (char) (~(inp[24] | inp[26]));
	const char v1 = (char) (inp[9] ^ inp[1]);
	const char v2 = (char) (inp[11]);
	const char v3 = (char) (inp[10] ^ inp[8]);
	const char v4 = (char) (~(inp[19] ^ inp[17]));
	const char v5 = (char) (~(inp[20] ^ inp[27]));
	const char v6 = (char) (~inp[0] | inp[6]);
	const char v7 = (char) (~inp[12]);
	const char v8 = (char) (~(inp[6] | inp[31]));
	const char v9 = (char) (inp[18]);
	const char v10 = (char) (inp[13]);
	const char v11 = (char) (inp[31] ^ inp[16]);
	const char v12 = (char) (inp[9]);
	const char v13 = (char) (inp[3] ^ inp[3]);
	const char v14 = (char) (~inp[21]);
	const char v15 = (char) (~inp[15]);
	const char v16 = (char) (inp[28] | inp[23]);
	const char v17 = (char) (inp[2] ^ inp[28]);
	const char v18 = (char) (inp[11] | inp[13]);
	const char v19 = (char) (inp[30]);
	const char v20 = (char) (~inp[22] | inp[2]);
	const char v21 = (char) (inp[8] & inp[7]);
	const char v22 = (char) (~(inp[0] ^ inp[24]));
	const char v23 = (char) (~inp[4]);
	const char v24 = (char) (inp[19] & ~inp[14]);
	const char v25 = (char) (~(inp[25] & inp[10]));
	const char v26 = (char) (~(inp[12] ^ inp[5]));
	const char v27 = (char) (~inp[1]);
	const char v28 = (char) (inp[29] ^ inp[17]);
	const char v29 = (char) (inp[16] & inp[22]);
	const char v30 = (char) (inp[20]);
	const char v31 = (char) (~(inp[4] & inp[5]));
	const char v32 = (char) (~v24 | v21);
	const char v33 = (char) (v23 & ~v4);
	const char v34 = (char) (~v26);
	const char v35 = (char) (~(v14 ^ v1));
	const char v36 = (char) (~v30);
	const char v37 = (char) (v17);
	const char v38 = (char) (~v9);
	const char v39 = (char) (v10 & ~v16);
	const char v40 = (char) (~v29);
	const char v41 = (char) (~v12 | v27);
	const char v42 = (char) (v21 | v22);
	const char v43 = (char) (v5 & ~v16);
	const char v44 = (char) (~v25);
	const char v45 = (char) (~(v28 ^ v20));
	const char v46 = (char) (~(v2 ^ v3));
	const char v47 = (char) (v13 | v10);
	const char v48 = (char) (~(v30 ^ v6));
	const char v49 = (char) (v28 ^ v23);
	const char v50 = (char) (v29 | v11);
	const char v51 = (char) (~v15 | v25);
	const char v52 = (char) (~v19);
	const char v53 = (char) (~v18);
	const char v54 = (char) (v7);
	const char v55 = (char) (v2);
	const char v56 = (char) (v18 & ~v8);
	const char v57 = (char) (~v1);
	const char v58 = (char) (v0);
	const char v59 = (char) (~v9);
	const char v60 = (char) (~(v6 ^ v24));
	const char v61 = (char) (~v11);
	const char v62 = (char) (v31 & ~v7);
	const char v63 = (char) (~v14);
	const char v64 = (char) (v52 | v52);
	const char v65 = (char) (v32);
	const char v66 = (char) (v45);
	const char v67 = (char) (~v45 | v57);
	const char v68 = (char) (~(v53 & v43));
	const char v69 = (char) (~v39);
	const char v70 = (char) (v47 & ~v58);
	const char v71 = (char) (~v39);
	const char v72 = (char) (~v59 | v48);
	const char v73 = (char) (v37 ^ v48);
	const char v74 = (char) (~v46 | v63);
	const char v75 = (char) (v55 ^ v49);
	const char v76 = (char) (~(v32 & v42));
	const char v77 = (char) (~(v44 & v43));
	const char v78 = (char) (v35);
	const char v79 = (char) (~v52);
	const char v80 = (char) (v41 | v40);
	const char v81 = (char) (v48 & ~v35);
	const char v82 = (char) (v53);
	const char v83 = (char) (~(v60 | v60));
	const char v84 = (char) (v59 ^ v46);
	const char v85 = (char) (~(v45 | v49));
	const char v86 = (char) (~v45);
	const char v87 = (char) (~(v58 ^ v34));
	const char v88 = (char) (~v37 | v55);
	const char v89 = (char) (~(v56 | v56));
	const char v90 = (char) (v47 ^ v32);
	const char v91 = (char) (~v36 | v37);
	const char v92 = (char) (~v47);
	const char v93 = (char) (v33 & v54);
	const char v94 = (char) (~v34);
	const char v95 = (char) (v61 ^ v62);
	const char v96 = (char) (v46);
	const char v97 = (char) (v62 & ~v44);
	const char v98 = (char) (v56);
	const char v99 = (char) (v58);
	const char v100 = (char) (v41 & ~v38);
	const char v101 = (char) (v33 ^ v54);
	const char v102 = (char) (~v44);
	const char v103 = (char) (v38 ^ v58);
	const char v104 = (char) (~v50);
	const char v105 = (char) (~(v61 | v38));
	const char v106 = (char) (v53);
	const char v107 = (char) (~v35);
	const char v108 = (char) (~(v55 | v37));
	const char v109 = (char) (~(v46 ^ v63));
	const char v110 = (char) (~v61);
	const char v111 = (char) (v42);
	const char v112 = (char) (~(v56 ^ v32));
	const char v113 = (char) (v38);
	const char v114 = (char) (v54 & v57);
	const char v115 = (char) (~(v46 ^ v57));
	const char v116 = (char) (v34 & v38);
	const char v117 = (char) (~v51 | v40);
	const char v118 = (char) (~(v51 ^ v59));
	const char v119 = (char) (~v32 | v51);
	const char v120 = (char) (~(v34 ^ v36));
	const char v121 = (char) (v32 & v59);
	const char v122 = (char) (~v36);
	const char v123 = (char) (v45 & ~v60);
	const char v124 = (char) (v60);
	const char v125 = (char) (v37 & v55);
	const char v126 = (char) (v40);
	const char v127 = (char) (~v61 | v39);
	const char v128 = (char) (~v50 | v41);
	const char v129 = (char) (v55);
	const char v130 = (char) (v40 | v54);
	const char v131 = (char) (~(v58 ^ v36));
	const char v132 = (char) (~(v38 ^ v57));
	const char v133 = (char) (~v62);
	const char v134 = (char) (v45);
	const char v135 = (char) (v36);
	const char v136 = (char) (v51 & ~v60);
	const char v137 = (char) (v54 ^ v58);
	const char v138 = (char) (~(v45 | v49));
	const char v139 = (char) (v39 | v58);
	const char v140 = (char) (~v34);
	const char v141 = (char) (v61);
	const char v142 = (char) (~v63);
	const char v143 = (char) (~(v39 ^ v34));
	const char v144 = (char) (~v51 | v42);
	const char v145 = (char) (~(v33 & v41));
	const char v146 = (char) (v51 & v49);
	const char v147 = (char) (v35 ^ v54);
	const char v148 = (char) (~v49);
	const char v149 = (char) (v33 & ~v37);
	const char v150 = (char) (~v33);
	const char v151 = (char) (v50);
	const char v152 = (char) (~(v52 & v34));
	const char v153 = (char) (~(v61 ^ v59));
	const char v154 = (char) (v46 & ~v58);
	const char v155 = (char) (~v44 | v56);
	const char v156 = (char) (v43);
	const char v157 = (char) (v43 & ~v62);
	const char v158 = (char) (~(v37 | v53));
	const char v159 = (char) (~v40);
	const char v160 = (char) (v62);
	const char v161 = (char) (v57 ^ v52);
	const char v162 = (char) (~v56);
	const char v163 = (char) (~(v42 ^ v59));
	const char v164 = (char) (v35 | v36);
	const char v165 = (char) (v48);
	const char v166 = (char) (~(v57 & v48));
	const char v167 = (char) (v52);
	const char v168 = (char) (v43);
	const char v169 = (char) (~(v62 ^ v42));
	const char v170 = (char) (v33);
	const char v171 = (char) (~v32 | v48);
	const char v172 = (char) (v50 & ~v39);
	const char v173 = (char) (v34 & v40);
	const char v174 = (char) (~v56);
	const char v175 = (char) (~(v32 & v41));
	const char v176 = (char) (v55 & v47);
	const char v177 = (char) (v62 & ~v40);
	const char v178 = (char) (~(v53 ^ v63));
	const char v179 = (char) (~v51 | v59);
	const char v180 = (char) (~v36);
	const char v181 = (char) (v49 & v35);
	const char v182 = (char) (~(v33 ^ v43));
	const char v183 = (char) (v63 & v52);
	const char v184 = (char) (~v60 | v43);
	const char v185 = (char) (~(v42 ^ v45));
	const char v186 = (char) (v61);
	const char v187 = (char) (v41);
	const char v188 = (char) (v44);
	const char v189 = (char) (v57);
	const char v190 = (char) (v35 ^ v51);
	const char v191 = (char) (~(v41 ^ v38));
	out[0] = (char) (~(v104 ^ v112));
	out[1] = (char) (v170);
	out[2] = (char) (~v174 | v108);
	out[3] = (char) (v80);
	out[4] = (char) (~v123);
	out[5] = (char) (v99 | v167);
	out[6] = (char) (~v116 | v144);
	out[7] = (char) (~(v113 ^ v186));
	out[8] = (char) (v121);
	out[9] = (char) (v164 | v143);
	out[10] = (char) (~(v118 | v78));
	out[11] = (char) (v136 ^ v158);
	out[12] = (char) (~(v95 & v185));
	out[13] = (char) (v155);
	out[14] = (char) (~(v174 | v104));
	out[15] = (char) (v189);
	out[16] = (char) (~(v189 | v87));
	out[17] = (char) (v137 ^ v127);
	out[18] = (char) (v148 & v145);
	out[19] = (char) (v72);
	out[20] = (char) (v191 ^ v166);
	out[21] = (char) (~v71 | v183);
	out[22] = (char) (v81);
	out[23] = (char) (v81 & ~v127);
	out[24] = (char) (~v88 | v83);
	out[25] = (char) (v117 ^ v171);
	out[26] = (char) (~v163);
	out[27] = (char) (v144 ^ v110);
	out[28] = (char) (v162);
	out[29] = (char) (~(v134 ^ v141));
	out[30] = (char) (v134);
	out[31] = (char) (v179 & v64);
	out[32] = (char) (v171 ^ v75);
	out[33] = (char) (v140 | v106);
	out[34] = (char) (~(v153 ^ v136));
	out[35] = (char) (v147 ^ v103);
	out[36] = (char) (v126 & v69);
	out[37] = (char) (v90);
	out[38] = (char) (~(v156 ^ v103));
	out[39] = (char) (~v169);
	out[40] = (char) (v153);
	out[41] = (char) (~(v188 ^ v173));
	out[42] = (char) (~(v152 ^ v105));
	out[43] = (char) (v120);
	out[44] = (char) (~v181);
	out[45] = (char) (v151 ^ v73);
	out[46] = (char) (v119);
	out[47] = (char) (v77 & v170);
	out[48] = (char) (~(v106 ^ v109));
	out[49] = (char) (~v91 | v70);
	out[50] = (char) (v79 ^ v131);
	out[51] = (char) (v84 | v177);
	out[52] = (char) (v169 & ~v95);
	out[53] = (char) (~v149 | v112);
	out[54] = (char) (~(v146 ^ v74));
	out[55] = (char) (~v185);
	out[56] = (char) (~v90 | v70);
	out[57] = (char) (v115);
	out[58] = (char) (v99 | v139);
	out[59] = (char) (~v69 | v156);
	out[60] = (char) (v71 ^ v132);
	out[61] = (char) (~v162 | v186);
	out[62] = (char) (~(v91 & v138));
	out[63] = (char) (v184 ^ v129);
	out[64] = (char) (~v154);
	out[65] = (char) (v164);
	out[66] = (char) (v119 & ~v166);
	out[67] = (char) (v128);
	out[68] = (char) (v165);
	out[69] = (char) (v64);
	out[70] = (char) (v102);
	out[71] = (char) (v68 ^ v73);
	out[72] = (char) (~v132 | v178);
	out[73] = (char) (~v131 | v92);
	out[74] = (char) (~(v163 | v85));
	out[75] = (char) (~v180);
	out[76] = (char) (v82 ^ v124);
	out[77] = (char) (~(v92 ^ v109));
	out[78] = (char) (~(v179 & v140));
	out[79] = (char) (~v168 | v182);
	out[80] = (char) (v113 & ~v102);
	out[81] = (char) (~(v111 ^ v133));
	out[82] = (char) (~v181);
	out[83] = (char) (~(v157 | v76));
	out[84] = (char) (v94 & ~v75);
	out[85] = (char) (v114 ^ v74);
	out[86] = (char) (~(v178 ^ v176));
	out[87] = (char) (~(v182 & v135));
	out[88] = (char) (~v159 | v122);
	out[89] = (char) (v93 ^ v96);
	out[90] = (char) (v148);
	out[91] = (char) (v190);
	out[92] = (char) (v154 & v65);
	out[93] = (char) (v161 & ~v187);
	out[94] = (char) (v72 ^ v150);
	out[95] = (char) (~v84);
	out[96] = (char) (v98 ^ v191);
	out[97] = (char) (v130 & v190);
	out[98] = (char) (~(v159 & v88));
	out[99] = (char) (v155);
	out[100] = (char) (v93 | v67);
	out[101] = (char) (~v128 | v67);
	out[102] = (char) (v77);
	out[103] = (char) (v180 | v107);
	out[104] = (char) (~(v120 ^ v97));
	out[105] = (char) (~(v147 & v129));
	out[106] = (char) (v175 & v135);
	out[107] = (char) (v100 | v130);
	out[108] = (char) (v110 & ~v145);
	out[109] = (char) (~v142);
	out[110] = (char) (~v161);
	out[111] = (char) (v133 ^ v79);
	out[112] = (char) (~(v160 | v86));
	out[113] = (char) (v141 & v66);
	out[114] = (char) (v123);
	out[115] = (char) (v138 ^ v89);
	out[116] = (char) (~(v160 | v149));
	out[117] = (char) (~v139);
	out[118] = (char) (~(v76 | v173));
	out[119] = (char) (v172);
	out[120] = (char) (v188 & ~v152);
	out[121] = (char) (v111 & ~v108);
	out[122] = (char) (v66);
	out[123] = (char) (v146);
	out[124] = (char) (v100);
	out[125] = (char) (~(v143 ^ v125));
	out[126] = (char) (v151 & ~v94);
	out[127] = (char) (~v175);
}

// void apply_logic_gate_net (bool const *inp, int *out, size_t len) {
//     char *inp_temp = malloc(32*sizeof(char));
//     char *out_temp = malloc(128*sizeof(char));
//     char *out_temp_o = malloc(5*sizeof(char));
    
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
//         for(size_t c = 0; c < 8; ++c) {  // for each class
//             // Initialize the output bits
//             for(size_t d = 0; d < 5; ++d) {
//                 out_temp_o[d] = (char) 0;
//             }
            
//             // Apply the adder logic gate network
//             for(size_t a = 0; a < 16; ++a) {
//                 char carry = out_temp[c * 16 + a];
//                 char out_temp_o_d;
//                 for(int d = 5 - 1; d >= 0; --d) {
//                     out_temp_o_d  = out_temp_o[d];
//                     out_temp_o[d] = carry ^ out_temp_o_d;
//                     carry         = carry & out_temp_o_d;
//                 }
//             }
            
//             // Unpack the result bits
//             for(size_t b = 0; b < 8; ++b) {
//                 const char bit_mask = (char) 1 << b;
//                 int res = 0;
//                 for(size_t d = 0; d < 5; ++d) {
//                     res <<= 1;
//                     res += !!(out_temp_o[d] & bit_mask);
//                 }
//                 out[(i * 8 + b) * 8 + c] = res;
//             }
//         }
//     }
//     free(inp_temp);
//     free(out_temp);
//     free(out_temp_o);
// }

void apply_logic_gate_net_singleval (char const *inp, int *out) {
    char *out_temp = (char*)malloc(128*sizeof(char));
    
    // for(size_t i = 0; i < len; ++i) {
    
        // Applying the logic gate net
        logic_gate_net(inp, out_temp);
				const int classSize = 128/8;

        for(size_t c = 0; c < 8; ++c) {  // for each class
					int classSum = 0;
					for(size_t node=c*classSize; node < (c*classSize) + classSize; node++) {
						classSum += out_temp[node] & 1; //take the lowest bit, ignore the rest	
					}
					out[c] = classSum;
				}
        
    // }
    free(out_temp);
}
