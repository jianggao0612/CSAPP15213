/* 
 * Gao Jiang (AndrewID: gaoj)
 *
 * Data Lab
 *
 */

//1
/*
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y) {
  int a = (~x) & y;
  int b = x & (~y);
  return ~((~a) & (~b));
}

/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
  return 1 << 31; // Tmin = 0x80000000
}
//2
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 2
 */
int isTmax(int x) {
  return !((~x^(x+1)) | !(~x));
}
/* 
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x) {
  int mask = (0xAA << 8) + 0xAA;
  mask = (mask << 16) + mask; //Get OxAAAAAAAA
  return !(~(x|(~mask)));  
}

/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  return ~x + 1;
}
//3
/* 
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x) {
   int lowerBound = 0x30;
   int upperBound = 0x3a;
   return !((x + (~lowerBound + 1)) >> 31) & (x + (~upperBound + 1)) >> 31; 
}
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
  return (y&(~((~(!x))+1))) + (z&((~(!x))+1));
}
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
  //Get the signs of x and y
  int signX = x >> 31;
  int signY = y >> 31;

  //Deal with the situation that x and y have different sign
  int flag1 = !signY & signX;
  
  //Determine whether x=y
  int flag2 = !(x ^ y);

  //Determin whether x<y when x and y have the same sign
  int flag3 = !(signX ^ signY) & ((x + (~y + 1)) >> 31 & 1);

  return flag1 | flag2 | flag3;
}
//4
/* 
 * logicalNeg - implement the ! operator, using all of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
  //Only 0 can result in a 1 in the sign bit when do ~x & (~(~x+1))
  int mask = ~x & (~(~x + 1));
  return mask >> 31 & 1; // Get 1 and 0 for 0 and all other numbers
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
  
  int isZeroOrNegone = !x | (!(x + 1));
  int firstShift = 0;
  int secondShift = 0; 
  int thirdShift = 0;
  int fourthShift = 0;
  int fifthShift = 0;
  int total = 0;

  // Get -x - 1
  x = x >> 31 ^ x;

  /*
   * Divide and Conquer
   * Find the highest bit of 1
   */
  firstShift = !(!(x>>16)) << 4;
  x >>= firstShift;

  secondShift = !(!(x>>8)) << 3;
  x >>= secondShift;

  thirdShift = !(!(x>>4)) << 2;
  x >>= thirdShift;

  fourthShift = !(!(x>>2)) << 1;
  x >>= fourthShift;

  fifthShift = 1;
  x >>= fifthShift;
 
  total = firstShift + secondShift + thirdShift + fourthShift + fifthShift + x + 1;

  return (isZeroOrNegone&(~((~(!isZeroOrNegone)) + 1))) + (total&((~(!isZeroOrNegone)) + 1)); // Use the conditional thoughts
}
//float
/* 
 * float_twice - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_twice(unsigned uf) {
  // Get the sign, exp, fraction bits
  int sign = uf & 0x80000000;
  int exp = (uf & 0x7F800000) >> 23;
  int frac = uf & 0x07FFFFF;

  int final_exp = 0;

  // Determine whether it's NaN
  if (exp == 0xFF){
    return uf;
  }	
  // Determine whether it's 0
  if((uf & 0x7fffffff) == 0){
    return uf;
  }
  // Determine whether it's denorm or norm
  if (!exp){
        //Denorm with no 1 in the highest bit in the fraction part - Left shift the fraction part by 1
	if( uf << 9 >> 31){
		//printf("denorm uf is %x.\n", (uf << 9 >> 8) + sign);	
		return (uf << 9 >> 8) + sign;
	}
	// Denorm with 1 in the highest bit in the fraction part - Left shift the exponent and fraction parts by 1
    	else{
		//printf("denorm uf shift is %x.\n", (uf << 1) + sign);
		return (uf << 1) + sign;
	}
   } else {
	 // Norm -  Add 1 to exp
	 final_exp = (exp + 1) << 23;
   // Consider the case whether the exponent bits become all ones - NaN
	 if (final_exp == 0x7f800000)
      frac = 0;
	 return ((exp + 1) << 23) + frac + sign;
   } 	 
}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {

	  // Get the sign bit
  	int sign = x & 0x80000000;
  	//printf("sign = %d\n", sign);
  	
	  // Store the highest bit with 1 in x
  	int frac = 0, exp = 0;

    // For bits needed to be discarded
  	int leftover = 0;
    int leftoverCnt = 0;

	  int fracMask = 0;
	  
    // For operations in the middle shift
    int mid = 0;
	  int shift = 0;
	  int inter = 0;
		
	  // Determine whether it's 0
  	if(!x)
		  return 0;
	
	  //Determine if it's tmin
  	if(x == 0x80000000) {
		  return 0xcf000000;
  	}	

  	// Determine whether x is positive or negative
  	if(sign == 0x80000000) {
		  // x is negative - set it to be positive
		  x = -x;
	  }

	  // Get highest bit of one
   	while( (x >> exp) != 1){
		  exp += 1;
  	}
 
    // Whether all the bits can be stored
  	if (exp > 23){
		  leftoverCnt = exp - 23;
		  shift = 1 << leftoverCnt;
		  leftover = shift - 1 & x;

		  frac = (x >> leftoverCnt) & 0x7fffff;
	
		  mid = shift>>1;
	
		  if(leftover > mid ) {
          inter = 1;
		  } else if (leftover == mid) {
			   inter = frac & 1;
		  }
		  frac = frac + inter;
	  } else{
		  frac = ((1 << exp) ^ x) << (23 - exp);
	  } 

	  if(frac == 0x800000) {
		  exp = exp + 1;
		  frac = 0;
	  }

  	exp = exp + 127;
	  exp = exp << 23;
	
	  return sign + exp + frac;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
  // Get sign, exponent and fraction
  int sign = uf & 0x80000000;
  int result = 0;
  int exp = (uf & 0x7f800000) >> 23;
  int frac = uf & 0x7fffff;
  int bias = 0x7F; // 2 to (8 - 1) - 1
  int e = 0;

  // Determine whether uf is NaN or infinite
  if(exp  == 0xff)
    return 0x80000000u;
  
  // If uf is 0 or denorm or norm with exp less than bias - round to 0
  if((exp < bias) || !(uf << 1)) 
    return 0;
  
  e = exp - bias;

  // If E is great than 31, the representation of integer will overflow
  if (e >= 31)
    return 0x80000000u;
  
  // Add back 1 in the norm case
  frac = frac | 0x800000;

  if(e < 23)
    result = frac >> (23 - e); // If e is less than 23, do right shift
  else
    result = frac << (e - 23); // If e is equal or great than 23, do left shift

  if (sign < 0)
    return -result;
  else
    return result;	
}
