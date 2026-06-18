#include<stdio.h>
#include <stdint.h>


size_t fib_multiply(const uint8_t *x, size_t m,
                      const uint8_t *y, size_t n,
                      uint8_t *z)
{
	uint64_t hold = 0;

    for (size_t k = 0; k < m + n; k++) {
        /* sum every product x[i]*y[j] with i + j == k */
        size_t i_lo = (k < n) ? 0 : k - (n - 1);   /* keep j = k - i < n */
        size_t i_hi = (k < m) ? k : m - 1;          /* keep i < m         */

        printf( "i_lo = %ld, i_hi =%ld\n", i_lo, i_hi);

        for (size_t i = i_lo; i <= i_hi; i++)
            hold += (uint64_t)x[i] * y[k - i];

        z[k] = (uint8_t)(hold % 10);
        hold /= 10;                                 /* carry into next column */
    }

    /* trim leading (most-significant) zeros, but keep at least one digit */
    size_t len = m + n;
    while (len > 1 && z[len - 1] == 0)
        len--;
    return len;
 }

 int main (int argc, char argv[])
 {
    uint8_t x_littleendian [] = {2,3};
    uint8_t y_littleendian [] = {5,6};

    uint8_t z[4];
    fib_multiply(&x_littleendian, 2, &y_littleendian, 2, &z);
    return 0;
 }

