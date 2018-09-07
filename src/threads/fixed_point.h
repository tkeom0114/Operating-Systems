/*Author:Taekang Eom
Time:09/06 21:04*/
#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H
typedef int fixed_t;
/*14 bits for fractional part*/
#define FRACTIONAL 14
/*convert integer to fixed point number*/
#define FIXED(N)((fixed_t)(N << FRACTIONAL))
/*take integer part of fixed point number*/
#define INT(X)(X >> FRACTIONAL)
/*convert fixed_point number to integer by rounding*/
#define ROUND(X)(X>=0 ? (X + (1 << (FRACTIONAL - 1))) >> FRACTIONAL \
                        : (X - (1 << (FRACTIONAL - 1))) >> FRACTIONAL)
/*add two fixed point numbers*/
#define FP_ADD(X,Y)(X + Y)
/*subtract two fixed point numbers*/
#define FP_MINUS(X,Y)(X - Y)
/*add fixed point number to integer*/
#define FP_MIXADD(X,N)(X + (N << FRACTIONAL))
/*subtract integer from fixed point number*/
#define FP_MIXMINUS(X,N)(X - (N << FRACTIONAL))
/*multiply two fixed point numbers*/
#define FP_MUL(X,Y)((fixed_t)((((int64_t) X) * Y) >> FRACTIONAL))
/*multiply fixed point number by integer*/
#define FP_MIXMUL(X,N)(X * N)
/*divide operation of two fixed point numbers*/
#define FP_DIV(X,Y)((fixed_t)((((int64_t) X) << FRACTIONAL) / Y))
/*divide fixed point number by integer*/
#define FP_MIXDIV(X,N)(X / N)

#endif
