#pragma once

// Rendererと将来のMeshが使用する頂点属性の組み合わせを表す。
enum class e_VertexFormat
{
	// Positionのみを持つ2Dまたは簡易Mesh。
	e_POSITION,
	// Positionと頂点Colorを持つMesh。
	e_POSITION_COLOR,
	// PositionとTexture UVを持つMesh。
	e_POSITION_TEXTURE,
	// Position、Normal、Texture UVを持つLit Mesh。
	e_POSITION_NORMAL_TEXTURE
};