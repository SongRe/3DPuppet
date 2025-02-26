-- Create torso node
root = gr.node("root")

left_joint = gr.joint("left", {-45, 0, 45}, {0, 0, 0})
root:add_child(left_joint)

left_box = gr.mesh("cube", "left_box")
left_joint:add_child(left_box)
left_box:scale(1.0, 0.5, 0.5)

pivot = gr.joint("left-mid-pivot", {-45, 0, 45}, {0, 0, 0})
pivot:translate(0.5, 0.0, 0.0)
left_joint:add_child(pivot)


mid_joint = gr.joint("mid", {-45, 0, 45}, {0, 0, 0})
mid_joint:translate(0.5, 0.0, 0.0)
pivot:add_child(mid_joint)

mid_box = gr.mesh("cube", "mid_box")
mid_box:scale(1.0, 0.5, 0.5)
mid_joint:add_child(mid_box)

root:translate(0.0, 0.0, -5)

return root