#define Reciprocal(value) (1.0 / ((f64)value))

#define Min(a, b) ((a) <= (b) ? (a) : (b))
#define Max(a, b) ((a) >= (b) ? (a) : (b))

#define Clamp(value, min, max) Max(Min(value, max), min)
