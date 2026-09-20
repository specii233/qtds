#version 330 core

// 直接把顶点色插值结果输出（暂无光照/纹理）
in vec3 vColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
