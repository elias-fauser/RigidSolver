#version 330

#define M_PI  3.14159265

uniform mat4 invModelViewMX;
uniform sampler3D volume;        //!< 3D texture handle 
uniform sampler1D transferTex;

uniform vec3 ambient;      //!< ambient color
uniform vec3 diffuse;      //!< diffuse color
uniform vec3 specular;     //!< specular color

uniform float k_amb;       //!< ambient factor
uniform float k_diff;      //!< diffuse factor
uniform float k_spec;      //!< specular factor
uniform float k_exp;       //!< specular exponent

uniform vec3 lightDirection;

in vec3 normal;
in vec2 texCoords;
in vec3 position;

// --------------------------------------------------
//   Blinn-Phong shading model
// --------------------------------------------------
vec3 blinnPhong(vec3 n, vec3 l, vec3 v) {
	
	vec3 h = normalize(v + l);

	vec3 Camb = k_amb * ambient;
	vec3 Cdiff = diffuse * k_diff * max(0.0, dot(n, l));
	vec3 Cspec = specular * k_spec * (k_exp + 2) / (2.0 * M_PI) * pow(max(0.0, dot(h, n)), k_exp);

    vec3 color = Camb + Cdiff + Cspec;

    return color;
}

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {
	
	vec4 cameraPosition = invModelViewMX[3];
	cameraPosition /= cameraPosition.w;
	
    vec3 view = normalize(position - cameraPosition.xyz);
	view = vec3(1, 1, 0);

    gl_FragColor = vec4(blinnPhong(normalize(normal), normalize(lightDirection), view), 1.0);
}
