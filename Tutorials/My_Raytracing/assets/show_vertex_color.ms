-- create a box and convert it to poly
b = $
-- instance a standardmaterial with vertexColor in diffuse and apply
m = standard()
m.diffusemap = vertex_color()
b.material = m
-- set the display modes to display vertex colors
setCVertMode b true
setShadeCVerts b true

-- required
select b
redrawViews()
-- ---
clearSelection()