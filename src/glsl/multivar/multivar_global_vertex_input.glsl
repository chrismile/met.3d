// Global inputs for line vertices
/*interface VSInput
{
    // Position in 3D world space
    layout(location = 0) vec3 vertexPosition;
    // Normal of current line segment
    layout(location = 1) vec3 vertexLineNormal;
    // Tangent of current line segment
    layout(location = 2) vec3 vertexLineTangent;
    // Description of current variable at line vertex
    layout(location = 3) vec4 multiVariable;
    // Description of line segment such as curve interpolant t, neighboring element:
    // (x: elementID, y: lineID, z: nextElementID, w: elementInterpolant)
    layout(location = 4) vec4 variableDesc;
};*/
interface VSInput
{
    // Position in 3D world space
    vec3 vertexPosition;
    // Normal of current line segment
    vec3 vertexLineNormal;
    // Tangent of current line segment
    vec3 vertexLineTangent;
    // Description of current variable at line vertex
    vec4 multiVariable;
    // Description of line segment such as curve interpolant t, neighboring element:
    // (x: elementID, y: lineID, z: nextElementID, w: elementInterpolant)
    vec4 variableDesc;
};
