#include "string.h"

#include "core/math/math_funcs.h"

std::string itos(int64_t p_number, int base, bool capitalize_hex) {
	bool sign = p_number < 0;

	int64_t n = p_number;

	int chars = 0;
	do {
		n /= base;
		chars++;
	} while (n);

	if (sign)
		chars++;
	std::string s;
	s.resize(chars + 1);
	char *c = &s[0];
	c[chars] = 0;
	n = p_number;
	do {
		int mod = ABS(n % base);
		if (mod >= 10) {
			char a = (capitalize_hex ? 'A' : 'a');
			c[--chars] = a + (mod - 10);
		} else {
			c[--chars] = '0' + mod;
		}

		n /= base;
	} while (n);

	if (sign)
		c[0] = '-';

	return s;
}

std::string rtos(double p_number, int p_decimals) {

	if (p_decimals > 16)
		p_decimals = 16;

	char fmt[7];
	fmt[0] = '%';
	fmt[1] = '.';

	if (p_decimals < 0) {

		fmt[1] = 'l';
		fmt[2] = 'f';
		fmt[3] = 0;

	} else if (p_decimals < 10) {
		fmt[2] = '0' + p_decimals;
		fmt[3] = 'l';
		fmt[4] = 'f';
		fmt[5] = 0;
	} else {
		fmt[2] = '0' + (p_decimals / 10);
		fmt[3] = '0' + (p_decimals % 10);
		fmt[4] = 'l';
		fmt[5] = 'f';
		fmt[6] = 0;
	}
	char buf[256];

#if defined(__GNUC__) || defined(_MSC_VER)
	snprintf(buf, 256, fmt, p_number);
#else
	sprintf(buf, fmt, p_num);
#endif

	buf[255] = 0;
	//destroy trailing zeroes
	{

		bool period = false;
		int z = 0;
		while (buf[z]) {
			if (buf[z] == '.')
				period = true;
			z++;
		}

		if (period) {
			z--;
			while (z > 0) {

				if (buf[z] == '0') {

					buf[z] = 0;
				} else if (buf[z] == '.') {

					buf[z] = 0;
					break;
				} else {

					break;
				}

				z--;
			}
		}
	}

	return buf;
}
