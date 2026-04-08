const SPH_PI: f32 = 3.14159265358979323846;

fn sph_poly6_kernel(r2: f32, h: f32) -> f32 {
    let h2 = h * h;
    if (r2 >= h2) {
        return 0.0;
    }

    let x = h2 - r2;
    let h9 = h * h * h * h * h * h * h * h * h;
    let coeff = 315.0 / (64.0 * SPH_PI * h9);
    return coeff * x * x * x;
}

fn sph_spiky_gradient(r: vec3f, h: f32) -> vec3f {
    let r_len = length(r);
    if (r_len <= 0.0 || r_len >= h) {
        return vec3f(0.0);
    }

    let h6 = h * h * h * h * h * h;
    let coeff = -45.0 / (SPH_PI * h6);
    let scale = coeff * (h - r_len) * (h - r_len) / r_len;
    return scale * r;
}

fn sph_viscosity_laplacian(r_len: f32, h: f32) -> f32 {
    if (r_len >= h) {
        return 0.0;
    }

    let h6 = h * h * h * h * h * h;
    let coeff = 45.0 / (SPH_PI * h6);
    return coeff * (h - r_len);
}
