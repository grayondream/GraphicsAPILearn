#version 330 core
in vec4 fragColor;
in vec2 textureCoord;

uniform bool enableFragCoord;
uniform bool enableFrontFaceCulling;

out vec4 color;
uniform sampler2D textureSampler;
void main(){
    color = fragColor;
    if(enableFragCoord){
        if(gl_FragCoord.x < 400)
            color = vec4(1.0, 0.0, 0.0, 1.0);
        else
            color = vec4(0.0, 1.0, 0.0, 1.0); 
    }

    if(enableFrontFaceCulling){
        if(gl_FrontFacing){
            color = vec4(0.0, 0.0, 1.0, 1.0);
        }else{  
            color = vec4(1.0, 1.0, 0.0, 1.0);
        }

    }
    //color = texture(textureSampler, textureCoord);
}