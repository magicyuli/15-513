/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
//1
/* 
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y) {
  /*
   * First, a ^ b = ~(~(a | b) | (a & b));
   * Second, a | b = ~(~a & ~b);
   * So, the answer is the first substituded | with second
   */
  int and = x & y;
  int or = ~(~x & ~y);
  int rev_or = ~or;
  int rev_or__or__and = ~(~rev_or & ~and);
  int xor = ~rev_or__or__and;
  return xor;
}
/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
  /*
   * The integer TMIN's bit representation should be 1 with 31 trailing 0s,
   * which can be obtained from left shifting 1 for 31 bits.
   */
  return 1 << 31;
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
  /*
   * To satisfy x + 1 == x ^ ~0, x has to be either -1 or tmax, because 
   * (x ^ ~0) + x = -1 and if x + 1 doesn't exceed tmax, the only x is -1.
   * So, I use !!(x ^ ~0) to exclude -1.
   */
  return !((x + 1) ^ (x ^ ~0)) & !!(x ^ ~0);
}
/* 
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x) {
  /*
   * First, I make the number with all odd-numbered bits set to 1.
   * Then, I convert all even-numbered bits to 0.
   * Finally, I test equality using xor and !.
   */
  int odd_ones = (((0x55 << 8 | 0x55) << 8 | 0x55) << 8 | 0x55) << 1;
  return !((x & odd_ones) ^ odd_ones);
}
/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  /*
   * -x = ~x + 1, because ~x + x == the number whose all bits are 1s,
   * which is -1.
   */
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
  /*
   * I compute x - 0x30 and x - 0x3a and test there sign bit.
   * Return True only if the former sign bit is 0 and latter is 1;
   */
  return !((x + (~0x30 + 1)) >> 31) & !!((x + (~0x3a + 1)) >> 31);
}
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
  /*
   * When x should be true, ~(!!x) + 1 == -1, ~(!x) + 1 == 0;
   * otherwise ~(!!x) + 1 == 0, ~(!x) + 1 == -1.
   * Thus, one from y_branch and z_branch will be 0, 
   * and the other one will be y or z.
   */
  int y_branch = (~(!!x) + 1) & y;
  int z_branch = (~(!x) + 1) & z;
  return y_branch | z_branch;
}
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
  /*
   * If x, y have the different signs and x is negtive and y is
   * positive, then it's true.
   * If x, y have the same signs, then simply return if x - (y + 1) < 0.
   * The sign judgement is to deal with overflow problem when big negative -
   * big positive == positive or vice versa.
   */
  int same_sign = !((x >> 31) ^ (y >> 31));
  return ((!same_sign) & (!!(x >> 31) & !(y >> 31)))
          | (same_sign & !!((x + (~(y + 1) + 1)) >> 31));
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
  /*
   * This promblem can be solved by testing if x is 0.
   * For all integers except 0 and tmin, the negatives have different
   * sign from the originals.
   * I use starts_with_one to exclude tmin.
   * So, x is true only when x and -x have different sign or x and -x have
   * the same sign but x starts with 1.
   * Finally simply return the negation of x.
   */
  int starts_with_one = x >> 31;
  int neg_x = ~x + 1;
  return ~(((neg_x >> 31) ^ (x >> 31)) | starts_with_one) & 1;
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
  /*
   * Do a binary search to search for the point where the high bits
   * which are all 1 or all 0 ends.
   * Start from the middle (point to middle), if all bits to the left are all 1 or 0,
   * then point to the middle of the right half, else point to the middle of the left half.
   * Repeat 6 times (log(32) + 1). The point of the last time of finding all 1 or 0 is the 
   * point of answer.
   */
  int all_0 = 0;
  int all_1 = ~0;
  int neg_8 = ~8 + 1;
  int neg_4 = neg_8 + 4;
  int neg_2 = neg_4 + 2;
  int neg_1 = all_1;
  // y is the number that has the same answer as x but with all 0 high bits
  int y = (x >> 31) ^ x;

  int res = 0;
  int offset = 16; 
  int offset_adj = 0;
  // erase all bits to the right of the current point
  int left_part = y >> offset;
  // is left part all 0
  int is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  // res = is_all_0 ? offset : res;
  res = (res & ~is_all_0) | (offset & is_all_0);
  // offset_adj = is_all_0 ? -8 : 8;
  offset_adj = (8 & ~is_all_0) | (neg_8 & is_all_0);

  /* second round */
  offset = offset + offset_adj;
  left_part = y >> offset;
  is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  res = (res & ~is_all_0) | (offset & is_all_0 );
  offset_adj = (4 & ~is_all_0) | (neg_4 & is_all_0);

  /* third round */
  offset = offset + offset_adj;
  left_part = y >> offset;
  is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  res = (res & ~is_all_0) | (offset & is_all_0);
  offset_adj = (2 & ~is_all_0) | (neg_2 & is_all_0);

  /* fourth round */
  offset = offset + offset_adj;
  left_part = y >> offset;
  is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  res = (res & ~is_all_0) | (offset & is_all_0);
  offset_adj = (1 & ~is_all_0) | (neg_1 & is_all_0);

  /* fifth round */
  offset = offset + offset_adj;
  left_part = y >> offset;
  is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  res = (res & ~is_all_0) | (offset & is_all_0);
  offset_adj = (1 & ~is_all_0) | (neg_1 & is_all_0);

  /* sixth round */
  offset = offset + offset_adj;
  left_part = y >> offset;
  is_all_0 = ~(!(all_0 ^ left_part)) + 1;
  res = (res & ~is_all_0) | (offset & is_all_0);

  // need +1 for the sign 
  return res + 1;
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
  unsigned sign_extractor = 0x80000000u;
  unsigned exp_extractor = 0x7f800000u;
  unsigned man_extractor = 0x007fffffu;
  // sign bits
  unsigned sign = uf & sign_extractor;
  // exponent bits
  unsigned exp = (uf & exp_extractor) >> 23;
  // mantissa bits
  unsigned man = uf & man_extractor;
  // preserve old mantissa for carry
  unsigned old_man = man;
  // not a number
  if (exp == 0xffu) {
    return uf;
  }
  // denormal number, just multiply mantissa by two.
  // if it turns into normal number, carry 1 to exponent.
  else if (exp == 0x00u) {
    man = man << 1;
    if (old_man > 0x400000u) {
      exp++;
    }
  }
  // just increase exp by 1 for normal numbers
  else {
    exp++;
  }
  // overflow
  if (exp == 0xffu) {
    man = 0u;
  }
  return sign | (exp << 23) | man;
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
  unsigned sign = 0x00000000u;
  unsigned exp = 0u;
  unsigned man = 0u;
  unsigned tmp_x;
  unsigned bits_to_discard;
  int discard_len;
  int discard_len_1;
  int man_pp;
  // return 0 if x is 0
  if (x) {
    if (x == 0x80000000) {
      return 0xcf000000u;
    }
    // convert negative to positive.
    if (x < 0) {
      sign = 0x80000000u;
      x = -x;
    }
    // preserve the original x
    tmp_x = x;
    // update exp while right shift x
    while (x > 1) {
      exp++;
      x >>= 1;
    }
    // if x is more than 23 bits, trailing bits have to be discarded;
    // if x is less than 23 bits, trailing 0s have to be added.
 
    man = (tmp_x & ((1 << exp) - 1)); 
    // add the trailing 0s
    if (exp < 23) {
      man <<= (23 - exp);
    }
    else {
      discard_len = exp - 23;
      discard_len_1 = exp - 24;
      bits_to_discard = man & ((1 << discard_len) - 1);
      man >>= discard_len;
      // bits_to_discard is half && mantissa is odd: man++
      // bits_to_discard is half && mantissa is even: do nothing
      // bits_to_discard is less than half: do nothing
      // bits_to_discard is more half: man++ 
      man_pp = 0;
      if (!(bits_to_discard ^ (1 << discard_len_1))) {
        if (man & 1) {
          man_pp = 1;
        }
      }
      else {
        if (bits_to_discard >> discard_len_1) {
          man_pp = 1;
        } 
      }
      if (man_pp) {
        man++;
      }
    }
    // after adding one, the mantissa may have grown to be greater or equal to 1
    if (man == 0x00800000u) {
      man = 0u;
      exp++;
    }
  
    return sign | ((exp + 127) << 23) | man;
  }
  else {
    return 0;
  }
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
  unsigned sign_extractor = 0x80000000u;
  unsigned exp_extractor = 0x7f800000u;
  unsigned man_extractor = 0x007fffffu;
  // sign bits
  unsigned sign = uf & sign_extractor;
  // exponent bits
  int exp = (uf & exp_extractor) >> 23;
  // mantissa bits
  unsigned man = uf & man_extractor;
  // not a number, inf
  if (exp == 0xffu) {
    return 0x80000000;
  }
  else if (exp == 0x00u) {
    return 0;
  }
  else {
    // number less than 1
    exp = exp - 127;
    if (exp < 0) {
      return 0;
    }
    // overflow int
    if (exp >= 31) {
      return 0x80000000;
    }
    // deal with mantissa
    if (exp >= 23) {
      man <<= (exp -23);
    }
    else {
      man >>= (23 - exp);
    }
    // take sign into consideration
    if (sign) {
      return -((1 << exp) | man);
    }
    return (1 << exp) | man;
  }
}
