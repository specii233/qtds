#version 330 core

// ---------------------------------------------------------------------------
// 顶点属性号必须与 myMesh.h 里的 kAttribPos / kAttribColor 一致：
//   位置 = 0，颜色 = 1（网格用 glVertexAttribFormat/glVertexAttribBinding 显式绑定）
// ---------------------------------------------------------------------------
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

// 投影 × 视图 × 模型：由 MyShaderProgram::setMat4("uMvp", ...) 每个模型写一次
uniform mat4 uMvp;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
