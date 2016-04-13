#include <stdlib.h>

/**
 * \brief Convert string to double indepedently on locale setting.
 *
 * \warning May change the input string
 */
static inline double atof_nolocale(char *str)
{
	char *endp;
	double d;
	d = strtod(str, &endp);
	if (*endp == ',') {
		*endp = '.';
	} else if  (*endp == '.') {
		*endp = ',';
	} else {
		return d;
	}
	d = strtod(str, NULL);
	return d;
}


