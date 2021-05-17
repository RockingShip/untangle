NODE box_0(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _03 = NODE(in0,in1,in1^IBIT);
NODE _14 = NODE(in2,in0,_03^IBIT);
NODE _20 = NODE(in3,_14,_14^IBIT);
return _20^IBIT;
}
NODE box_1(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _03 = NODE(in0,in1,in1^IBIT);
NODE _15 = NODE(in1,in2,in2^IBIT);
NODE _16 = NODE(in2,_03,in0^IBIT);
NODE _21 = NODE(in3,_15,_16);
return _21;
}
NODE box_2(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _03 = NODE(in0,in1,in1^IBIT);
NODE _15 = NODE(in1,in2,in2^IBIT);
NODE _17 = NODE(in2,in0,_03);
NODE _22 = NODE(in3,_17,_15);
return _22;
}
NODE box_3(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _03 = NODE(in0,in1,in1^IBIT);
NODE _04 = NODE(in0,in1,0);
NODE _18 = NODE(in2,in1,_03);
NODE _19 = NODE(in2,_04,_04^IBIT);
NODE _23 = NODE(in3,_18,_19^IBIT);
return _23^IBIT;
}
NODE box_4(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _01 = NODE(in4,in5,in5^IBIT);
NODE _05 = NODE(in6,in4,_01^IBIT);
NODE _10 = NODE(in7,_05,_05^IBIT);
return _10^IBIT;
}
NODE box_5(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _00 = NODE(in5,in6,in6^IBIT);
NODE _01 = NODE(in4,in5,in5^IBIT);
NODE _06 = NODE(in6,_01,in4^IBIT);
NODE _11 = NODE(in7,_00,_06);
return _11;
}
NODE box_6(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _00 = NODE(in5,in6,in6^IBIT);
NODE _01 = NODE(in4,in5,in5^IBIT);
NODE _07 = NODE(in6,in4,_01);
NODE _12 = NODE(in7,_07,_00);
return _12;
}
NODE box_7(NODE in7, NODE in6, NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) { 
NODE _01 = NODE(in4,in5,in5^IBIT);
NODE _02 = NODE(in4,in5,0);
NODE _08 = NODE(in6,in5,_01);
NODE _09 = NODE(in6,_02,_02^IBIT);
NODE _13 = NODE(in7,_08,_09^IBIT);
return _13^IBIT;
}
