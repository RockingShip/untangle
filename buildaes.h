/*
 * buildaes.h
 * 	Ancient code that creates the input database.
 * 	Names of keys, roots/entrypoints and intermediates
 * 	Note: nodeId #1 is used for "un-initialised" error
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2021, xyzzy@rockingship.org
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
enum {
	kZero, // reference value
	kError, // un-initialised marker

	// input keys
	k000, k001, k002, k003, k004, k005, k006, k007,
	k010, k011, k012, k013, k014, k015, k016, k017,
	k020, k021, k022, k023, k024, k025, k026, k027,
	k030, k031, k032, k033, k034, k035, k036, k037,
	k100, k101, k102, k103, k104, k105, k106, k107,
	k110, k111, k112, k113, k114, k115, k116, k117,
	k120, k121, k122, k123, k124, k125, k126, k127,
	k130, k131, k132, k133, k134, k135, k136, k137,
	k200, k201, k202, k203, k204, k205, k206, k207,
	k210, k211, k212, k213, k214, k215, k216, k217,
	k220, k221, k222, k223, k224, k225, k226, k227,
	k230, k231, k232, k233, k234, k235, k236, k237,
	k300, k301, k302, k303, k304, k305, k306, k307,
	k310, k311, k312, k313, k314, k315, k316, k317,
	k320, k321, k322, k323, k324, k325, k326, k327,
	k330, k331, k332, k333, k334, k335, k336, k337,

	// input keys
	i000, i001, i002, i003, i004, i005, i006, i007,
	i010, i011, i012, i013, i014, i015, i016, i017,
	i020, i021, i022, i023, i024, i025, i026, i027,
	i030, i031, i032, i033, i034, i035, i036, i037,
	i100, i101, i102, i103, i104, i105, i106, i107,
	i110, i111, i112, i113, i114, i115, i116, i117,
	i120, i121, i122, i123, i124, i125, i126, i127,
	i130, i131, i132, i133, i134, i135, i136, i137,
	i200, i201, i202, i203, i204, i205, i206, i207,
	i210, i211, i212, i213, i214, i215, i216, i217,
	i220, i221, i222, i223, i224, i225, i226, i227,
	i230, i231, i232, i233, i234, i235, i236, i237,
	i300, i301, i302, i303, i304, i305, i306, i307,
	i310, i311, i312, i313, i314, i315, i316, i317,
	i320, i321, i322, i323, i324, i325, i326, i327,
	i330, i331, i332, i333, i334, i335, i336, i337,

	// output roots
	o000, o001, o002, o003, o004, o005, o006, o007,
	o010, o011, o012, o013, o014, o015, o016, o017,
	o020, o021, o022, o023, o024, o025, o026, o027,
	o030, o031, o032, o033, o034, o035, o036, o037,
	o100, o101, o102, o103, o104, o105, o106, o107,
	o110, o111, o112, o113, o114, o115, o116, o117,
	o120, o121, o122, o123, o124, o125, o126, o127,
	o130, o131, o132, o133, o134, o135, o136, o137,
	o200, o201, o202, o203, o204, o205, o206, o207,
	o210, o211, o212, o213, o214, o215, o216, o217,
	o220, o221, o222, o223, o224, o225, o226, o227,
	o230, o231, o232, o233, o234, o235, o236, o237,
	o300, o301, o302, o303, o304, o305, o306, o307,
	o310, o311, o312, o313, o314, o315, o316, o317,
	o320, o321, o322, o323, o324, o325, o326, o327,
	o330, o331, o332, o333, o334, o335, o336, o337,

	/*
	 * NOTE: NSTART of the main tree starts here, the following are offsets for `V[]` and optional extended keys
	 */

	// All the intermediate rounds, in order of being generated
	k0700, k0701, k0702, k0703, k0704, k0705, k0706, k0707, k1130, k1131, k1132, k1133, k1134, k1135, k1136, k1137,
	k1520, k1521, k1522, k1523, k1524, k1525, k1526, k1527, k1910, k1911, k1912, k1913, k1914, k1915, k1916, k1917,
	k2300, k2301, k2302, k2303, k2304, k2305, k2306, k2307, k2730, k2731, k2732, k2733, k2734, k2735, k2736, k2737,
	k3120, k3121, k3122, k3123, k3124, k3125, k3126, k3127, k3510, k3511, k3512, k3513, k3514, k3515, k3516, k3517,
	k3900, k3901, k3902, k3903, k3904, k3905, k3906, k3907, k4330, k4331, k4332, k4333, k4334, k4335, k4336, k4337,
	k0730, k0731, k0732, k0733, k0734, k0735, k0736, k0737, k1120, k1121, k1122, k1123, k1124, k1125, k1126, k1127,
	k1510, k1511, k1512, k1513, k1514, k1515, k1516, k1517, k1900, k1901, k1902, k1903, k1904, k1905, k1906, k1907,
	k2330, k2331, k2332, k2333, k2334, k2335, k2336, k2337, k2720, k2721, k2722, k2723, k2724, k2725, k2726, k2727,
	k3110, k3111, k3112, k3113, k3114, k3115, k3116, k3117, k3500, k3501, k3502, k3503, k3504, k3505, k3506, k3507,
	k3930, k3931, k3932, k3933, k3934, k3935, k3936, k3937, k4320, k4321, k4322, k4323, k4324, k4325, k4326, k4327,
	k0720, k0721, k0722, k0723, k0724, k0725, k0726, k0727, k1110, k1111, k1112, k1113, k1114, k1115, k1116, k1117,
	k1500, k1501, k1502, k1503, k1504, k1505, k1506, k1507, k1930, k1931, k1932, k1933, k1934, k1935, k1936, k1937,
	k2320, k2321, k2322, k2323, k2324, k2325, k2326, k2327, k2710, k2711, k2712, k2713, k2714, k2715, k2716, k2717,
	k3100, k3101, k3102, k3103, k3104, k3105, k3106, k3107, k3530, k3531, k3532, k3533, k3534, k3535, k3536, k3537,
	k3920, k3921, k3922, k3923, k3924, k3925, k3926, k3927, k4310, k4311, k4312, k4313, k4314, k4315, k4316, k4317,
	k0710, k0711, k0712, k0713, k0714, k0715, k0716, k0717, k1100, k1101, k1102, k1103, k1104, k1105, k1106, k1107,
	k1530, k1531, k1532, k1533, k1534, k1535, k1536, k1537, k1920, k1921, k1922, k1923, k1924, k1925, k1926, k1927,
	k2310, k2311, k2312, k2313, k2314, k2315, k2316, k2317, k2700, k2701, k2702, k2703, k2704, k2705, k2706, k2707,
	k3130, k3131, k3132, k3133, k3134, k3135, k3136, k3137, k3520, k3521, k3522, k3523, k3524, k3525, k3526, k3527,
	k3910, k3911, k3912, k3913, k3914, k3915, k3916, k3917, k4300, k4301, k4302, k4303, k4304, k4305, k4306, k4307,

	v0000, v0001, v0002, v0003, v0004, v0005, v0006, v0007, v0100, v0101, v0102, v0103, v0104, v0105, v0106, v0107,
	v0200, v0201, v0202, v0203, v0204, v0205, v0206, v0207, v0300, v0301, v0302, v0303, v0304, v0305, v0306, v0307,
	v0010, v0011, v0012, v0013, v0014, v0015, v0016, v0017, v0110, v0111, v0112, v0113, v0114, v0115, v0116, v0117,
	v0210, v0211, v0212, v0213, v0214, v0215, v0216, v0217, v0310, v0311, v0312, v0313, v0314, v0315, v0316, v0317,
	v0020, v0021, v0022, v0023, v0024, v0025, v0026, v0027, v0120, v0121, v0122, v0123, v0124, v0125, v0126, v0127,
	v0220, v0221, v0222, v0223, v0224, v0225, v0226, v0227, v0320, v0321, v0322, v0323, v0324, v0325, v0326, v0327,
	v0030, v0031, v0032, v0033, v0034, v0035, v0036, v0037, v0130, v0131, v0132, v0133, v0134, v0135, v0136, v0137,
	v0230, v0231, v0232, v0233, v0234, v0235, v0236, v0237, v0330, v0331, v0332, v0333, v0334, v0335, v0336, v0337,
	v1000, v1001, v1002, v1003, v1004, v1005, v1006, v1007, v1100, v1101, v1102, v1103, v1104, v1105, v1106, v1107,
	v1200, v1201, v1202, v1203, v1204, v1205, v1206, v1207, v1300, v1301, v1302, v1303, v1304, v1305, v1306, v1307,
	v1010, v1011, v1012, v1013, v1014, v1015, v1016, v1017, v1110, v1111, v1112, v1113, v1114, v1115, v1116, v1117,
	v1210, v1211, v1212, v1213, v1214, v1215, v1216, v1217, v1310, v1311, v1312, v1313, v1314, v1315, v1316, v1317,
	v1020, v1021, v1022, v1023, v1024, v1025, v1026, v1027, v1120, v1121, v1122, v1123, v1124, v1125, v1126, v1127,
	v1220, v1221, v1222, v1223, v1224, v1225, v1226, v1227, v1320, v1321, v1322, v1323, v1324, v1325, v1326, v1327,
	v1030, v1031, v1032, v1033, v1034, v1035, v1036, v1037, v1130, v1131, v1132, v1133, v1134, v1135, v1136, v1137,
	v1230, v1231, v1232, v1233, v1234, v1235, v1236, v1237, v1330, v1331, v1332, v1333, v1334, v1335, v1336, v1337,
	v2000, v2001, v2002, v2003, v2004, v2005, v2006, v2007, v2100, v2101, v2102, v2103, v2104, v2105, v2106, v2107,
	v2200, v2201, v2202, v2203, v2204, v2205, v2206, v2207, v2300, v2301, v2302, v2303, v2304, v2305, v2306, v2307,
	v2010, v2011, v2012, v2013, v2014, v2015, v2016, v2017, v2110, v2111, v2112, v2113, v2114, v2115, v2116, v2117,
	v2210, v2211, v2212, v2213, v2214, v2215, v2216, v2217, v2310, v2311, v2312, v2313, v2314, v2315, v2316, v2317,
	v2020, v2021, v2022, v2023, v2024, v2025, v2026, v2027, v2120, v2121, v2122, v2123, v2124, v2125, v2126, v2127,
	v2220, v2221, v2222, v2223, v2224, v2225, v2226, v2227, v2320, v2321, v2322, v2323, v2324, v2325, v2326, v2327,
	v2030, v2031, v2032, v2033, v2034, v2035, v2036, v2037, v2130, v2131, v2132, v2133, v2134, v2135, v2136, v2137,
	v2230, v2231, v2232, v2233, v2234, v2235, v2236, v2237, v2330, v2331, v2332, v2333, v2334, v2335, v2336, v2337,
	v3000, v3001, v3002, v3003, v3004, v3005, v3006, v3007, v3100, v3101, v3102, v3103, v3104, v3105, v3106, v3107,
	v3200, v3201, v3202, v3203, v3204, v3205, v3206, v3207, v3300, v3301, v3302, v3303, v3304, v3305, v3306, v3307,
	v3010, v3011, v3012, v3013, v3014, v3015, v3016, v3017, v3110, v3111, v3112, v3113, v3114, v3115, v3116, v3117,
	v3210, v3211, v3212, v3213, v3214, v3215, v3216, v3217, v3310, v3311, v3312, v3313, v3314, v3315, v3316, v3317,
	v3020, v3021, v3022, v3023, v3024, v3025, v3026, v3027, v3120, v3121, v3122, v3123, v3124, v3125, v3126, v3127,
	v3220, v3221, v3222, v3223, v3224, v3225, v3226, v3227, v3320, v3321, v3322, v3323, v3324, v3325, v3326, v3327,
	v3030, v3031, v3032, v3033, v3034, v3035, v3036, v3037, v3130, v3131, v3132, v3133, v3134, v3135, v3136, v3137,
	v3230, v3231, v3232, v3233, v3234, v3235, v3236, v3237, v3330, v3331, v3332, v3333, v3334, v3335, v3336, v3337,
	v4000, v4001, v4002, v4003, v4004, v4005, v4006, v4007, v4100, v4101, v4102, v4103, v4104, v4105, v4106, v4107,
	v4200, v4201, v4202, v4203, v4204, v4205, v4206, v4207, v4300, v4301, v4302, v4303, v4304, v4305, v4306, v4307,
	v4010, v4011, v4012, v4013, v4014, v4015, v4016, v4017, v4110, v4111, v4112, v4113, v4114, v4115, v4116, v4117,
	v4210, v4211, v4212, v4213, v4214, v4215, v4216, v4217, v4310, v4311, v4312, v4313, v4314, v4315, v4316, v4317,
	v4020, v4021, v4022, v4023, v4024, v4025, v4026, v4027, v4120, v4121, v4122, v4123, v4124, v4125, v4126, v4127,
	v4220, v4221, v4222, v4223, v4224, v4225, v4226, v4227, v4320, v4321, v4322, v4323, v4324, v4325, v4326, v4327,
	v4030, v4031, v4032, v4033, v4034, v4035, v4036, v4037, v4130, v4131, v4132, v4133, v4134, v4135, v4136, v4137,
	v4230, v4231, v4232, v4233, v4234, v4235, v4236, v4237, v4330, v4331, v4332, v4333, v4334, v4335, v4336, v4337,
	v5000, v5001, v5002, v5003, v5004, v5005, v5006, v5007, v5100, v5101, v5102, v5103, v5104, v5105, v5106, v5107,
	v5200, v5201, v5202, v5203, v5204, v5205, v5206, v5207, v5300, v5301, v5302, v5303, v5304, v5305, v5306, v5307,
	v5010, v5011, v5012, v5013, v5014, v5015, v5016, v5017, v5110, v5111, v5112, v5113, v5114, v5115, v5116, v5117,
	v5210, v5211, v5212, v5213, v5214, v5215, v5216, v5217, v5310, v5311, v5312, v5313, v5314, v5315, v5316, v5317,
	v5020, v5021, v5022, v5023, v5024, v5025, v5026, v5027, v5120, v5121, v5122, v5123, v5124, v5125, v5126, v5127,
	v5220, v5221, v5222, v5223, v5224, v5225, v5226, v5227, v5320, v5321, v5322, v5323, v5324, v5325, v5326, v5327,
	v5030, v5031, v5032, v5033, v5034, v5035, v5036, v5037, v5130, v5131, v5132, v5133, v5134, v5135, v5136, v5137,
	v5230, v5231, v5232, v5233, v5234, v5235, v5236, v5237, v5330, v5331, v5332, v5333, v5334, v5335, v5336, v5337,
	v6000, v6001, v6002, v6003, v6004, v6005, v6006, v6007, v6100, v6101, v6102, v6103, v6104, v6105, v6106, v6107,
	v6200, v6201, v6202, v6203, v6204, v6205, v6206, v6207, v6300, v6301, v6302, v6303, v6304, v6305, v6306, v6307,
	v6010, v6011, v6012, v6013, v6014, v6015, v6016, v6017, v6110, v6111, v6112, v6113, v6114, v6115, v6116, v6117,
	v6210, v6211, v6212, v6213, v6214, v6215, v6216, v6217, v6310, v6311, v6312, v6313, v6314, v6315, v6316, v6317,
	v6020, v6021, v6022, v6023, v6024, v6025, v6026, v6027, v6120, v6121, v6122, v6123, v6124, v6125, v6126, v6127,
	v6220, v6221, v6222, v6223, v6224, v6225, v6226, v6227, v6320, v6321, v6322, v6323, v6324, v6325, v6326, v6327,
	v6030, v6031, v6032, v6033, v6034, v6035, v6036, v6037, v6130, v6131, v6132, v6133, v6134, v6135, v6136, v6137,
	v6230, v6231, v6232, v6233, v6234, v6235, v6236, v6237, v6330, v6331, v6332, v6333, v6334, v6335, v6336, v6337,
	v7000, v7001, v7002, v7003, v7004, v7005, v7006, v7007, v7100, v7101, v7102, v7103, v7104, v7105, v7106, v7107,
	v7200, v7201, v7202, v7203, v7204, v7205, v7206, v7207, v7300, v7301, v7302, v7303, v7304, v7305, v7306, v7307,
	v7010, v7011, v7012, v7013, v7014, v7015, v7016, v7017, v7110, v7111, v7112, v7113, v7114, v7115, v7116, v7117,
	v7210, v7211, v7212, v7213, v7214, v7215, v7216, v7217, v7310, v7311, v7312, v7313, v7314, v7315, v7316, v7317,
	v7020, v7021, v7022, v7023, v7024, v7025, v7026, v7027, v7120, v7121, v7122, v7123, v7124, v7125, v7126, v7127,
	v7220, v7221, v7222, v7223, v7224, v7225, v7226, v7227, v7320, v7321, v7322, v7323, v7324, v7325, v7326, v7327,
	v7030, v7031, v7032, v7033, v7034, v7035, v7036, v7037, v7130, v7131, v7132, v7133, v7134, v7135, v7136, v7137,
	v7230, v7231, v7232, v7233, v7234, v7235, v7236, v7237, v7330, v7331, v7332, v7333, v7334, v7335, v7336, v7337,
	v8000, v8001, v8002, v8003, v8004, v8005, v8006, v8007, v8100, v8101, v8102, v8103, v8104, v8105, v8106, v8107,
	v8200, v8201, v8202, v8203, v8204, v8205, v8206, v8207, v8300, v8301, v8302, v8303, v8304, v8305, v8306, v8307,
	v8010, v8011, v8012, v8013, v8014, v8015, v8016, v8017, v8110, v8111, v8112, v8113, v8114, v8115, v8116, v8117,
	v8210, v8211, v8212, v8213, v8214, v8215, v8216, v8217, v8310, v8311, v8312, v8313, v8314, v8315, v8316, v8317,
	v8020, v8021, v8022, v8023, v8024, v8025, v8026, v8027, v8120, v8121, v8122, v8123, v8124, v8125, v8126, v8127,
	v8220, v8221, v8222, v8223, v8224, v8225, v8226, v8227, v8320, v8321, v8322, v8323, v8324, v8325, v8326, v8327,
	v8030, v8031, v8032, v8033, v8034, v8035, v8036, v8037, v8130, v8131, v8132, v8133, v8134, v8135, v8136, v8137,
	v8230, v8231, v8232, v8233, v8234, v8235, v8236, v8237, v8330, v8331, v8332, v8333, v8334, v8335, v8336, v8337,
	v9000, v9001, v9002, v9003, v9004, v9005, v9006, v9007, v9100, v9101, v9102, v9103, v9104, v9105, v9106, v9107,
	v9200, v9201, v9202, v9203, v9204, v9205, v9206, v9207, v9300, v9301, v9302, v9303, v9304, v9305, v9306, v9307,
	v9010, v9011, v9012, v9013, v9014, v9015, v9016, v9017, v9110, v9111, v9112, v9113, v9114, v9115, v9116, v9117,
	v9210, v9211, v9212, v9213, v9214, v9215, v9216, v9217, v9310, v9311, v9312, v9313, v9314, v9315, v9316, v9317,
	v9020, v9021, v9022, v9023, v9024, v9025, v9026, v9027, v9120, v9121, v9122, v9123, v9124, v9125, v9126, v9127,
	v9220, v9221, v9222, v9223, v9224, v9225, v9226, v9227, v9320, v9321, v9322, v9323, v9324, v9325, v9326, v9327,
	v9030, v9031, v9032, v9033, v9034, v9035, v9036, v9037, v9130, v9131, v9132, v9133, v9134, v9135, v9136, v9137,
	v9230, v9231, v9232, v9233, v9234, v9235, v9236, v9237, v9330, v9331, v9332, v9333, v9334, v9335, v9336, v9337,

	ELAST, // last variable

	KSTART = k000, // first input
	OSTART = o000, // first output
	ESTART = k0700, // first intermediate
};

const char* allNames[] = {
	"ZERO", // reference value
	"ERROR", // un-initialised marker

	// input keys
	"k000","k001","k002","k003","k004","k005","k006","k007",
	"k010","k011","k012","k013","k014","k015","k016","k017",
	"k020","k021","k022","k023","k024","k025","k026","k027",
	"k030","k031","k032","k033","k034","k035","k036","k037",
	"k100","k101","k102","k103","k104","k105","k106","k107",
	"k110","k111","k112","k113","k114","k115","k116","k117",
	"k120","k121","k122","k123","k124","k125","k126","k127",
	"k130","k131","k132","k133","k134","k135","k136","k137",
	"k200","k201","k202","k203","k204","k205","k206","k207",
	"k210","k211","k212","k213","k214","k215","k216","k217",
	"k220","k221","k222","k223","k224","k225","k226","k227",
	"k230","k231","k232","k233","k234","k235","k236","k237",
	"k300","k301","k302","k303","k304","k305","k306","k307",
	"k310","k311","k312","k313","k314","k315","k316","k317",
	"k320","k321","k322","k323","k324","k325","k326","k327",
	"k330","k331","k332","k333","k334","k335","k336","k337",

	// input keys
	"i000","i001","i002","i003","i004","i005","i006","i007",
	"i010","i011","i012","i013","i014","i015","i016","i017",
	"i020","i021","i022","i023","i024","i025","i026","i027",
	"i030","i031","i032","i033","i034","i035","i036","i037",
	"i100","i101","i102","i103","i104","i105","i106","i107",
	"i110","i111","i112","i113","i114","i115","i116","i117",
	"i120","i121","i122","i123","i124","i125","i126","i127",
	"i130","i131","i132","i133","i134","i135","i136","i137",
	"i200","i201","i202","i203","i204","i205","i206","i207",
	"i210","i211","i212","i213","i214","i215","i216","i217",
	"i220","i221","i222","i223","i224","i225","i226","i227",
	"i230","i231","i232","i233","i234","i235","i236","i237",
	"i300","i301","i302","i303","i304","i305","i306","i307",
	"i310","i311","i312","i313","i314","i315","i316","i317",
	"i320","i321","i322","i323","i324","i325","i326","i327",
	"i330","i331","i332","i333","i334","i335","i336","i337",

	// output roots
	"o000","o001","o002","o003","o004","o005","o006","o007",
	"o010","o011","o012","o013","o014","o015","o016","o017",
	"o020","o021","o022","o023","o024","o025","o026","o027",
	"o030","o031","o032","o033","o034","o035","o036","o037",
	"o100","o101","o102","o103","o104","o105","o106","o107",
	"o110","o111","o112","o113","o114","o115","o116","o117",
	"o120","o121","o122","o123","o124","o125","o126","o127",
	"o130","o131","o132","o133","o134","o135","o136","o137",
	"o200","o201","o202","o203","o204","o205","o206","o207",
	"o210","o211","o212","o213","o214","o215","o216","o217",
	"o220","o221","o222","o223","o224","o225","o226","o227",
	"o230","o231","o232","o233","o234","o235","o236","o237",
	"o300","o301","o302","o303","o304","o305","o306","o307",
	"o310","o311","o312","o313","o314","o315","o316","o317",
	"o320","o321","o322","o323","o324","o325","o326","o327",
	"o330","o331","o332","o333","o334","o335","o336","o337",

	// All the intermediate rounds
	"k0700","k0701","k0702","k0703","k0704","k0705","k0706","k0707","k1130","k1131","k1132","k1133","k1134","k1135","k1136","k1137",
	"k1520","k1521","k1522","k1523","k1524","k1525","k1526","k1527","k1910","k1911","k1912","k1913","k1914","k1915","k1916","k1917",
	"k2300","k2301","k2302","k2303","k2304","k2305","k2306","k2307","k2730","k2731","k2732","k2733","k2734","k2735","k2736","k2737",
	"k3120","k3121","k3122","k3123","k3124","k3125","k3126","k3127","k3510","k3511","k3512","k3513","k3514","k3515","k3516","k3517",
	"k3900","k3901","k3902","k3903","k3904","k3905","k3906","k3907","k4330","k4331","k4332","k4333","k4334","k4335","k4336","k4337",
	"k0730","k0731","k0732","k0733","k0734","k0735","k0736","k0737","k1120","k1121","k1122","k1123","k1124","k1125","k1126","k1127",
	"k1510","k1511","k1512","k1513","k1514","k1515","k1516","k1517","k1900","k1901","k1902","k1903","k1904","k1905","k1906","k1907",
	"k2330","k2331","k2332","k2333","k2334","k2335","k2336","k2337","k2720","k2721","k2722","k2723","k2724","k2725","k2726","k2727",
	"k3110","k3111","k3112","k3113","k3114","k3115","k3116","k3117","k3500","k3501","k3502","k3503","k3504","k3505","k3506","k3507",
	"k3930","k3931","k3932","k3933","k3934","k3935","k3936","k3937","k4320","k4321","k4322","k4323","k4324","k4325","k4326","k4327",
	"k0720","k0721","k0722","k0723","k0724","k0725","k0726","k0727","k1110","k1111","k1112","k1113","k1114","k1115","k1116","k1117",
	"k1500","k1501","k1502","k1503","k1504","k1505","k1506","k1507","k1930","k1931","k1932","k1933","k1934","k1935","k1936","k1937",
	"k2320","k2321","k2322","k2323","k2324","k2325","k2326","k2327","k2710","k2711","k2712","k2713","k2714","k2715","k2716","k2717",
	"k3100","k3101","k3102","k3103","k3104","k3105","k3106","k3107","k3530","k3531","k3532","k3533","k3534","k3535","k3536","k3537",
	"k3920","k3921","k3922","k3923","k3924","k3925","k3926","k3927","k4310","k4311","k4312","k4313","k4314","k4315","k4316","k4317",
	"k0710","k0711","k0712","k0713","k0714","k0715","k0716","k0717","k1100","k1101","k1102","k1103","k1104","k1105","k1106","k1107",
	"k1530","k1531","k1532","k1533","k1534","k1535","k1536","k1537","k1920","k1921","k1922","k1923","k1924","k1925","k1926","k1927",
	"k2310","k2311","k2312","k2313","k2314","k2315","k2316","k2317","k2700","k2701","k2702","k2703","k2704","k2705","k2706","k2707",
	"k3130","k3131","k3132","k3133","k3134","k3135","k3136","k3137","k3520","k3521","k3522","k3523","k3524","k3525","k3526","k3527",
	"k3910","k3911","k3912","k3913","k3914","k3915","k3916","k3917","k4300","k4301","k4302","k4303","k4304","k4305","k4306","k4307",

	"v0000","v0001","v0002","v0003","v0004","v0005","v0006","v0007","v0100","v0101","v0102","v0103","v0104","v0105","v0106","v0107",
	"v0200","v0201","v0202","v0203","v0204","v0205","v0206","v0207","v0300","v0301","v0302","v0303","v0304","v0305","v0306","v0307",
	"v0010","v0011","v0012","v0013","v0014","v0015","v0016","v0017","v0110","v0111","v0112","v0113","v0114","v0115","v0116","v0117",
	"v0210","v0211","v0212","v0213","v0214","v0215","v0216","v0217","v0310","v0311","v0312","v0313","v0314","v0315","v0316","v0317",
	"v0020","v0021","v0022","v0023","v0024","v0025","v0026","v0027","v0120","v0121","v0122","v0123","v0124","v0125","v0126","v0127",
	"v0220","v0221","v0222","v0223","v0224","v0225","v0226","v0227","v0320","v0321","v0322","v0323","v0324","v0325","v0326","v0327",
	"v0030","v0031","v0032","v0033","v0034","v0035","v0036","v0037","v0130","v0131","v0132","v0133","v0134","v0135","v0136","v0137",
	"v0230","v0231","v0232","v0233","v0234","v0235","v0236","v0237","v0330","v0331","v0332","v0333","v0334","v0335","v0336","v0337",
	"v1000","v1001","v1002","v1003","v1004","v1005","v1006","v1007","v1100","v1101","v1102","v1103","v1104","v1105","v1106","v1107",
	"v1200","v1201","v1202","v1203","v1204","v1205","v1206","v1207","v1300","v1301","v1302","v1303","v1304","v1305","v1306","v1307",
	"v1010","v1011","v1012","v1013","v1014","v1015","v1016","v1017","v1110","v1111","v1112","v1113","v1114","v1115","v1116","v1117",
	"v1210","v1211","v1212","v1213","v1214","v1215","v1216","v1217","v1310","v1311","v1312","v1313","v1314","v1315","v1316","v1317",
	"v1020","v1021","v1022","v1023","v1024","v1025","v1026","v1027","v1120","v1121","v1122","v1123","v1124","v1125","v1126","v1127",
	"v1220","v1221","v1222","v1223","v1224","v1225","v1226","v1227","v1320","v1321","v1322","v1323","v1324","v1325","v1326","v1327",
	"v1030","v1031","v1032","v1033","v1034","v1035","v1036","v1037","v1130","v1131","v1132","v1133","v1134","v1135","v1136","v1137",
	"v1230","v1231","v1232","v1233","v1234","v1235","v1236","v1237","v1330","v1331","v1332","v1333","v1334","v1335","v1336","v1337",
	"v2000","v2001","v2002","v2003","v2004","v2005","v2006","v2007","v2100","v2101","v2102","v2103","v2104","v2105","v2106","v2107",
	"v2200","v2201","v2202","v2203","v2204","v2205","v2206","v2207","v2300","v2301","v2302","v2303","v2304","v2305","v2306","v2307",
	"v2010","v2011","v2012","v2013","v2014","v2015","v2016","v2017","v2110","v2111","v2112","v2113","v2114","v2115","v2116","v2117",
	"v2210","v2211","v2212","v2213","v2214","v2215","v2216","v2217","v2310","v2311","v2312","v2313","v2314","v2315","v2316","v2317",
	"v2020","v2021","v2022","v2023","v2024","v2025","v2026","v2027","v2120","v2121","v2122","v2123","v2124","v2125","v2126","v2127",
	"v2220","v2221","v2222","v2223","v2224","v2225","v2226","v2227","v2320","v2321","v2322","v2323","v2324","v2325","v2326","v2327",
	"v2030","v2031","v2032","v2033","v2034","v2035","v2036","v2037","v2130","v2131","v2132","v2133","v2134","v2135","v2136","v2137",
	"v2230","v2231","v2232","v2233","v2234","v2235","v2236","v2237","v2330","v2331","v2332","v2333","v2334","v2335","v2336","v2337",
	"v3000","v3001","v3002","v3003","v3004","v3005","v3006","v3007","v3100","v3101","v3102","v3103","v3104","v3105","v3106","v3107",
	"v3200","v3201","v3202","v3203","v3204","v3205","v3206","v3207","v3300","v3301","v3302","v3303","v3304","v3305","v3306","v3307",
	"v3010","v3011","v3012","v3013","v3014","v3015","v3016","v3017","v3110","v3111","v3112","v3113","v3114","v3115","v3116","v3117",
	"v3210","v3211","v3212","v3213","v3214","v3215","v3216","v3217","v3310","v3311","v3312","v3313","v3314","v3315","v3316","v3317",
	"v3020","v3021","v3022","v3023","v3024","v3025","v3026","v3027","v3120","v3121","v3122","v3123","v3124","v3125","v3126","v3127",
	"v3220","v3221","v3222","v3223","v3224","v3225","v3226","v3227","v3320","v3321","v3322","v3323","v3324","v3325","v3326","v3327",
	"v3030","v3031","v3032","v3033","v3034","v3035","v3036","v3037","v3130","v3131","v3132","v3133","v3134","v3135","v3136","v3137",
	"v3230","v3231","v3232","v3233","v3234","v3235","v3236","v3237","v3330","v3331","v3332","v3333","v3334","v3335","v3336","v3337",
	"v4000","v4001","v4002","v4003","v4004","v4005","v4006","v4007","v4100","v4101","v4102","v4103","v4104","v4105","v4106","v4107",
	"v4200","v4201","v4202","v4203","v4204","v4205","v4206","v4207","v4300","v4301","v4302","v4303","v4304","v4305","v4306","v4307",
	"v4010","v4011","v4012","v4013","v4014","v4015","v4016","v4017","v4110","v4111","v4112","v4113","v4114","v4115","v4116","v4117",
	"v4210","v4211","v4212","v4213","v4214","v4215","v4216","v4217","v4310","v4311","v4312","v4313","v4314","v4315","v4316","v4317",
	"v4020","v4021","v4022","v4023","v4024","v4025","v4026","v4027","v4120","v4121","v4122","v4123","v4124","v4125","v4126","v4127",
	"v4220","v4221","v4222","v4223","v4224","v4225","v4226","v4227","v4320","v4321","v4322","v4323","v4324","v4325","v4326","v4327",
	"v4030","v4031","v4032","v4033","v4034","v4035","v4036","v4037","v4130","v4131","v4132","v4133","v4134","v4135","v4136","v4137",
	"v4230","v4231","v4232","v4233","v4234","v4235","v4236","v4237","v4330","v4331","v4332","v4333","v4334","v4335","v4336","v4337",
	"v5000","v5001","v5002","v5003","v5004","v5005","v5006","v5007","v5100","v5101","v5102","v5103","v5104","v5105","v5106","v5107",
	"v5200","v5201","v5202","v5203","v5204","v5205","v5206","v5207","v5300","v5301","v5302","v5303","v5304","v5305","v5306","v5307",
	"v5010","v5011","v5012","v5013","v5014","v5015","v5016","v5017","v5110","v5111","v5112","v5113","v5114","v5115","v5116","v5117",
	"v5210","v5211","v5212","v5213","v5214","v5215","v5216","v5217","v5310","v5311","v5312","v5313","v5314","v5315","v5316","v5317",
	"v5020","v5021","v5022","v5023","v5024","v5025","v5026","v5027","v5120","v5121","v5122","v5123","v5124","v5125","v5126","v5127",
	"v5220","v5221","v5222","v5223","v5224","v5225","v5226","v5227","v5320","v5321","v5322","v5323","v5324","v5325","v5326","v5327",
	"v5030","v5031","v5032","v5033","v5034","v5035","v5036","v5037","v5130","v5131","v5132","v5133","v5134","v5135","v5136","v5137",
	"v5230","v5231","v5232","v5233","v5234","v5235","v5236","v5237","v5330","v5331","v5332","v5333","v5334","v5335","v5336","v5337",
	"v6000","v6001","v6002","v6003","v6004","v6005","v6006","v6007","v6100","v6101","v6102","v6103","v6104","v6105","v6106","v6107",
	"v6200","v6201","v6202","v6203","v6204","v6205","v6206","v6207","v6300","v6301","v6302","v6303","v6304","v6305","v6306","v6307",
	"v6010","v6011","v6012","v6013","v6014","v6015","v6016","v6017","v6110","v6111","v6112","v6113","v6114","v6115","v6116","v6117",
	"v6210","v6211","v6212","v6213","v6214","v6215","v6216","v6217","v6310","v6311","v6312","v6313","v6314","v6315","v6316","v6317",
	"v6020","v6021","v6022","v6023","v6024","v6025","v6026","v6027","v6120","v6121","v6122","v6123","v6124","v6125","v6126","v6127",
	"v6220","v6221","v6222","v6223","v6224","v6225","v6226","v6227","v6320","v6321","v6322","v6323","v6324","v6325","v6326","v6327",
	"v6030","v6031","v6032","v6033","v6034","v6035","v6036","v6037","v6130","v6131","v6132","v6133","v6134","v6135","v6136","v6137",
	"v6230","v6231","v6232","v6233","v6234","v6235","v6236","v6237","v6330","v6331","v6332","v6333","v6334","v6335","v6336","v6337",
	"v7000","v7001","v7002","v7003","v7004","v7005","v7006","v7007","v7100","v7101","v7102","v7103","v7104","v7105","v7106","v7107",
	"v7200","v7201","v7202","v7203","v7204","v7205","v7206","v7207","v7300","v7301","v7302","v7303","v7304","v7305","v7306","v7307",
	"v7010","v7011","v7012","v7013","v7014","v7015","v7016","v7017","v7110","v7111","v7112","v7113","v7114","v7115","v7116","v7117",
	"v7210","v7211","v7212","v7213","v7214","v7215","v7216","v7217","v7310","v7311","v7312","v7313","v7314","v7315","v7316","v7317",
	"v7020","v7021","v7022","v7023","v7024","v7025","v7026","v7027","v7120","v7121","v7122","v7123","v7124","v7125","v7126","v7127",
	"v7220","v7221","v7222","v7223","v7224","v7225","v7226","v7227","v7320","v7321","v7322","v7323","v7324","v7325","v7326","v7327",
	"v7030","v7031","v7032","v7033","v7034","v7035","v7036","v7037","v7130","v7131","v7132","v7133","v7134","v7135","v7136","v7137",
	"v7230","v7231","v7232","v7233","v7234","v7235","v7236","v7237","v7330","v7331","v7332","v7333","v7334","v7335","v7336","v7337",
	"v8000","v8001","v8002","v8003","v8004","v8005","v8006","v8007","v8100","v8101","v8102","v8103","v8104","v8105","v8106","v8107",
	"v8200","v8201","v8202","v8203","v8204","v8205","v8206","v8207","v8300","v8301","v8302","v8303","v8304","v8305","v8306","v8307",
	"v8010","v8011","v8012","v8013","v8014","v8015","v8016","v8017","v8110","v8111","v8112","v8113","v8114","v8115","v8116","v8117",
	"v8210","v8211","v8212","v8213","v8214","v8215","v8216","v8217","v8310","v8311","v8312","v8313","v8314","v8315","v8316","v8317",
	"v8020","v8021","v8022","v8023","v8024","v8025","v8026","v8027","v8120","v8121","v8122","v8123","v8124","v8125","v8126","v8127",
	"v8220","v8221","v8222","v8223","v8224","v8225","v8226","v8227","v8320","v8321","v8322","v8323","v8324","v8325","v8326","v8327",
	"v8030","v8031","v8032","v8033","v8034","v8035","v8036","v8037","v8130","v8131","v8132","v8133","v8134","v8135","v8136","v8137",
	"v8230","v8231","v8232","v8233","v8234","v8235","v8236","v8237","v8330","v8331","v8332","v8333","v8334","v8335","v8336","v8337",
	"v9000","v9001","v9002","v9003","v9004","v9005","v9006","v9007","v9100","v9101","v9102","v9103","v9104","v9105","v9106","v9107",
	"v9200","v9201","v9202","v9203","v9204","v9205","v9206","v9207","v9300","v9301","v9302","v9303","v9304","v9305","v9306","v9307",
	"v9010","v9011","v9012","v9013","v9014","v9015","v9016","v9017","v9110","v9111","v9112","v9113","v9114","v9115","v9116","v9117",
	"v9210","v9211","v9212","v9213","v9214","v9215","v9216","v9217","v9310","v9311","v9312","v9313","v9314","v9315","v9316","v9317",
	"v9020","v9021","v9022","v9023","v9024","v9025","v9026","v9027","v9120","v9121","v9122","v9123","v9124","v9125","v9126","v9127",
	"v9220","v9221","v9222","v9223","v9224","v9225","v9226","v9227","v9320","v9321","v9322","v9323","v9324","v9325","v9326","v9327",
	"v9030","v9031","v9032","v9033","v9034","v9035","v9036","v9037","v9130","v9131","v9132","v9133","v9134","v9135","v9136","v9137",
	"v9230","v9231","v9232","v9233","v9234","v9235","v9236","v9237","v9330","v9331","v9332","v9333","v9334","v9335","v9336","v9337"
};
