
#include<assert.h>

//
//		Count Set Bit
//

static unsigned count_set_bit(unsigned int x)
{
	int count = 0;
	while (x)
	{
		x = x & (x - 1);
		++count;
	}
	return count;
}

//
//	Count Set Bit - Table Based Approach
//

//
//  integers = 0,1,2,3,4,5,6,7,8,9,10,11,12,13............255
//  set bits = 0,1,1,2,1,2,2,3,1,2,2,3,2,3
//

#define B1(n) n,     n+1         // 2
#define B2(n) B1(n), B1(n+1)     // 4
#define B3(n) B2(n), B2(n+1)     // 4+4=8
#define B4(n) B3(n), B3(n+1)     // 8+8=16
#define B5(n) B4(n), B4(n+1)     //  32
#define B6(n) B5(n), B5(n+1)     //  64
#define B7(n) B6(n), B6(n+1)     //  128

//
// get n bits from position p
//
unsigned int getbits(unsigned int x, int p /*0 based index in bitmap */, int n)
{
	unsigned int mask	= ~(~0 << n);
	unsigned int shift  = (p - n + 1);
	unsigned int value = x >> shift;
	return (value & mask);
}

void uint_test_count_bit()
{
	const unsigned int value = 11;
	unsigned int yy = count_set_bit(value);
	assert(yy == 3);
}

void unit_test_get_bit()
{
	//
	// unit test for getbits
	//
	unsigned int xx = getbits(12, 3, 2);
	assert(xx == 3);
}

void unit_test_table_based_bit_count()
{
	// table based bit count
	static unsigned int table_lookup[] = { B7(0), B7(1) };
	unsigned int number_of_elements = sizeof(table_lookup) / sizeof(unsigned int);
	assert(number_of_elements == 256);

	const unsigned int value = 11;
	unsigned int yy = count_set_bit(value);

	unsigned int yy_table = table_lookup[value & 0xff] +
		table_lookup[(value >> 8) & 0xff] +
		table_lookup[(value >> 16) & 0xff] +
		table_lookup[value >> 24];

	assert(yy_table == yy);
}

int main()
{
	uint_test_count_bit();
	unit_test_get_bit();
	unit_test_table_based_bit_count();
	return 0;
}

