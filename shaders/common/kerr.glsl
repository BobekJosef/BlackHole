#ifndef KERR_GLSL
#define KERR_GLSL

#include "frame.glsl"

// Null geodesics of the Kerr metric in outgoing Kerr-Schild Cartesian coordinates, M = 1.
// Float copy of src/physics/Geodesic.cpp and KerrSchild.cpp, which hold the derivation and are
// unit-tested in double precision: keep the two in step.
//
//   g = eta + f l l,  f = 2 r^3 / (r^4 + a^2 z^2),
//   l_mu = (1, -(r x - a y) / (r^2 + a^2), -(r y + a x) / (r^2 + a^2), -z / r),
//   H = 1/2 (-p_t^2 + |p|^2 - f L^2),  L = -p_t + l . p,
//   dx/dlambda = p - f L l,   dp/dlambda = 1/2 L^2 grad f + f L grad(l . p).

// Boyer-Lindquist r: root of r^4 - (|x|^2 - a^2) r^2 - a^2 z^2 = 0.
float kerrRadius(vec3 x)
{
    float a2 = uSpin * uSpin;
    float b = dot(x, x) - a2;
    return sqrt(0.5 * (b + sqrt(b * b + 4.0 * a2 * x.z * x.z)));
}

struct PhotonDerivative {
    vec3 dx;
    vec3 dp;
};

PhotonDerivative geodesicDerivative(vec3 x, vec3 p, float pt)
{
    // Same algebra as Geodesic.cpp, arranged around three reciprocals (1/r, 1/s, 1/q): divisions
    // are several times the cost of multiplications, and this function runs 4x per step per pixel.
    float a2 = uSpin * uSpin;
    float b = -uSpin; // outgoing l_i = -(ingoing l_i with a -> -a)
    float z2 = x.z * x.z;

    float rho2 = dot(x, x) - a2;
    float r2 = 0.5 * (rho2 + sqrt(rho2 * rho2 + 4.0 * a2 * z2));
    float invR = inversesqrt(r2);
    float r = r2 * invR;
    float s = r2 + a2;
    float invS = 1.0 / s;
    float invQ = 1.0 / (r2 * r2 + a2 * z2);
    float f = 2.0 * r2 * r * invQ;

    vec3 l = -vec3((r * x.x + b * x.y) * invS, (r * x.y - b * x.x) * invS, x.z * invR);
    float bigL = -pt + dot(l, p);

    vec3 gradR = vec3(r2 * r * x.x, r2 * r * x.y, r * x.z * s) * invQ;
    float fInvQ = f * invQ;
    vec3 gradF = (3.0 * a2 * z2 - r2 * r2) * invR * fInvQ * gradR - vec3(0.0, 0.0, 2.0 * a2 * x.z * fInvQ);

    float bigA = x.x * p.x + x.y * p.y;
    float bigB = x.y * p.x - x.x * p.y;
    float c = (bigA - 2.0 * r * (r * bigA + b * bigB) * invS) * invS - x.z * p.z * invR * invR;
    vec3 gradLp = -(c * gradR + vec3((r * p.x - b * p.y) * invS, (r * p.y + b * p.x) * invS, p.z * invR));

    float fL = f * bigL;
    PhotonDerivative d;
    d.dx = p - fL * l;
    d.dp = 0.5 * bigL * bigL * gradF + fL * gradLp;
    return d;
}

// Relative violation of the null condition, |H| / |p|^2: the integration error.
float constraintError(vec3 x, vec3 p, float pt)
{
    float a2 = uSpin * uSpin;
    float r = kerrRadius(x);
    float r2 = r * r;
    float s = r2 + a2;
    float f = 2.0 * r2 * r / (r2 * r2 + a2 * x.z * x.z);
    vec3 l = -vec3(r * x.x - uSpin * x.y, r * x.y + uSpin * x.x, x.z * s / r) / s;
    float bigL = -pt + dot(l, p);
    float h = 0.5 * (-pt * pt + dot(p, p) - f * bigL * bigL);
    return abs(h) / max(dot(p, p), 1.0e-20);
}

// One classical Runge-Kutta step of affine length h.
void rungeKuttaStep(inout vec3 x, inout vec3 p, float pt, float h)
{
    PhotonDerivative k1 = geodesicDerivative(x, p, pt);
    PhotonDerivative k2 = geodesicDerivative(x + 0.5 * h * k1.dx, p + 0.5 * h * k1.dp, pt);
    PhotonDerivative k3 = geodesicDerivative(x + 0.5 * h * k2.dx, p + 0.5 * h * k2.dp, pt);
    PhotonDerivative k4 = geodesicDerivative(x + h * k3.dx, p + h * k3.dp, pt);
    x += h / 6.0 * (k1.dx + 2.0 * k2.dx + 2.0 * k3.dx + k4.dx);
    p += h / 6.0 * (k1.dp + 2.0 * k2.dp + 2.0 * k3.dp + k4.dp);
}

#endif
