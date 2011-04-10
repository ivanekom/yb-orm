
#ifndef YB__DECIMAL_H__INCLUDED
#define YB__DECIMAL_H__INCLUDED

#include <exception>
#include <string>
#include <iosfwd>

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 decimal_numerator;
#else
typedef long long decimal_numerator;
#endif

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define MAX_DECIMAL_NUMERATOR 999999999999999999
#else
#define MAX_DECIMAL_NUMERATOR 999999999999999999LL
#endif

const int MAX_DECIMAL_LENGTH = 18;

class decimal
{
public:
	class exception: public std::exception
	{
	public:
		virtual const char *what() const throw();
	};

	class overflow: public exception
	{
	public:
		virtual const char *what() const throw();
	};

	class divizion_by_zero: public exception
	{
	public:
		virtual const char *what() const throw();
	};

	class invalid_format: public exception
	{
	public:
		virtual const char *what() const throw();
	};

public:
	decimal(int x = 0, int p = 0);
	decimal(decimal_numerator x, int p = 0);
	decimal(const char *s);
	decimal(const std::string &s);
	decimal(double x);

	decimal &operator += (const decimal &x);
	decimal &operator -= (const decimal &x);
	decimal &operator *= (const decimal &x);
	decimal &operator /= (const decimal &x);
	decimal &operator ++ ();
	const decimal operator ++ (int);
	decimal &operator -- ();
	const decimal operator -- (int);
	const decimal operator - () const;
	bool is_positive() const;
	bool eq(const decimal &x) const;
	bool lt(const decimal &x) const;

	decimal &round(int p = 0);
	const decimal round(int p = 0) const;
	decimal_numerator ipart() const;
	decimal_numerator fpart(int p) const;
	inline int get_precision() const { return precision_; }
	inline decimal_numerator get_value() const { return value_; }
	const std::string str() const;

private:
	decimal_numerator value_;
	int precision_;
};

const decimal operator + (const decimal &, const decimal &);
const decimal operator - (const decimal &, const decimal &);
const decimal operator * (const decimal &, const decimal &);
const decimal operator / (const decimal &, const decimal &);
std::ostream &operator << (std::ostream &o, const decimal &x);
std::istream &operator >> (std::istream &i, decimal &x);

inline bool operator == (const decimal &x, const decimal &y) { return x.eq(y); }
inline bool operator != (const decimal &x, const decimal &y) { return !x.eq(y); }
inline bool operator < (const decimal &x, const decimal &y) { return x.lt(y); }
inline bool operator > (const decimal &x, const decimal &y) { return y.lt(x); }
inline bool operator >= (const decimal &x, const decimal &y) { return !x.lt(y); }
inline bool operator <= (const decimal &x, const decimal &y) { return !y.lt(x); }

inline const decimal decimal2(const std::string &s) { return decimal(s).round(2); }
inline const decimal decimal4(const std::string &s) { return decimal(s).round(4); }

// vim:ts=4:sts=4:sw=4:noet

#endif

