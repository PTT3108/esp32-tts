#ifndef _BV
#define _BV(b) (1UL << (b))
#endif
#define ENABLED(V...)       DO(ENA,&&,V)
#define TBI(N,B) (N ^= _BV(B))
#define _BV32(b) (1UL << (b))
#define TEST32(n,b) !!((n)&_BV32(b))
#define SBI32(n,b) (n |= _BV32(b))
#define CBI32(n,b) (n &= ~_BV32(b))
#define TBI32(N,B) (N ^= _BV32(B))
#define GPIO_CFG_NUN(a)     ((gpio_num_t)a)

#define SET_BIT_TO(N,B,TF) do{ if (TF) SBI(N,B); else CBI(N,B); }while(0)
#ifndef SBI
  #define SBI(A,B) (A |= _BV(B))
#endif
#ifndef CBI
  #define CBI(A,B) (A &= ~_BV(B))
#endif
#define TEST(n,b) (!!((n)&_BV(b)))
// Value helper macros
#define WITHIN(N,L,H)       ((N) >= (L) && (N) <= (H))
#define ISEOL(C)            ((C) == '\n' || (C) == '\r')
#define NUMERIC(a)          WITHIN(a, '0', '9')
#define DECIMAL(a)          (NUMERIC(a) || a == '.')
#define HEXCHR(a)           (NUMERIC(a) ? (a) - '0' : WITHIN(a, 'a', 'f') ? ((a) - 'a' + 10)  : WITHIN(a, 'A', 'F') ? ((a) - 'A' + 10) : -1)
#define NUMERIC_SIGNED(a)   (NUMERIC(a) || (a) == '-' || (a) == '+')
#define DECIMAL_SIGNED(a)   (DECIMAL(a) || (a) == '-' || (a) == '+')
