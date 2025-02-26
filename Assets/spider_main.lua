
-- Create the top-level root node.
root = gr.node("root")

-- ===================== TORSO =================================
-- torso joint
torso_joint = gr.joint("torso_joint", {0, 0, 0}, {0, 0, 0})
root:add_child(torso_joint)

torso = gr.mesh("cube", "torso")
torso:set_material(gr.material({0.25, 0.02, 0.02}, {0.5, 0.5, 0.5}, 100))
-- Scale the torso to a rectangular shape.
torso:scale(0.9, 0.7, 1.2)
torso_joint:translate(0.0, 0.0, 0.0)
-- The torso geometry is added as a leaf to the torso joint.
torso_joint:add_child(torso)

-- ====================== HEAD ===============================

-- Create a head joint to allow head articulation relative to the torso.
head_joint = gr.joint("head_joint", {-20, 0, 20}, {-30, 0, 30})
-- Attach the head joint as a child of the torso joint.
torso_joint:add_child(head_joint)

-- Create the head geometry as a sphere.
head = gr.mesh("cube", "head")
head:set_material(gr.material({0.1, 0.1, 0.1}, {0.8, 0.8, 0.8}, 50))
-- Scale the head down.
head:scale(1.0, 0.9, 0.8)

-- Translate the head relative to the head joint so it sits above the torso.
head_joint:translate(0.0, 0.0, 1.0)
head_joint:add_child(head)

-- EYES for head
function create_eye(name, tx, ty, tz, r, g, b)
    eye = gr.mesh("cube", name .. "_head_eye")
    eye:set_material(gr.material({r, g, b}, {0.8, 0.8, 0.8}, 50))
    head:add_child(eye)
    eye:translate(tx, ty, tz)
    eye:scale(0.15, 0.15, 0.15)
end

local eye_z = 3.2
local detail_z = 3.2

function create_detail(name, tx, ty, tz, r, g, b)
    detail = gr.mesh("cube", name .. "_head_detail")
    detail:set_material(gr.material({r, g, b}, {0.8, 0.8, 0.8}, 50))
    head:add_child(detail)
    detail:translate(tx, ty, tz)
    detail:scale(0.10, 0.15, 0.15)
end

create_eye("eye1", -1.5, 0.5, eye_z, 0.75, 0.05, 0.05)
create_eye("eye2", -0.5, 0.5, eye_z, 0.25, 0.02, 0.02)
create_eye("eye3", -0.5, -0.5, eye_z, 0.75, 0.05, 0.05)
create_eye("eye4", -1.5, -0.5, eye_z, 0.25, 0.02, 0.02)
create_eye("eye5", 1.5, 0.5, eye_z, 0.75, 0.05, 0.05)
create_eye("eye6", 0.5, -0.5, eye_z, 0.25, 0.02, 0.02)
create_eye("eye7", 1.5, -0.5, eye_z, 0.25, 0.05, 0.05)
create_eye("eye8", 0.5, 0.5, eye_z, 0.75, 0.02, 0.02)

create_detail("detail1", -4.4, 1.5, detail_z, 0.25, 0.02, 0.02)
create_detail("detail2", -3.2, 2.8, detail_z, 0.75, 0.02, 0.02)
create_detail("detail2", -2.2, 1.8, detail_z, 0.5, 0.02, 0.02)
create_detail("detail3", 4.4, 1.5, detail_z, 0.25, 0.02, 0.02)
create_detail("detail4", 3.2, 2.8, detail_z, 0.5, 0.02, 0.02)
create_detail("detail5", 2.2, 1.8, detail_z, 0.75, 0.02, 0.02)



-- ================================ BACK ======================================
back_joint = gr.joint("back_joint", {-20, 0, 20}, {-30, 0, 30})
torso_joint:add_child(back_joint)

back = gr.mesh("cube", "back")
back:set_material(gr.material({0.25, 0.02, 0.02}, {0.5, 0.5, 0.5}, 100))
back_joint:add_child(back)
back:scale(1.4, 0.8, 1.4)
back:translate(0.0, 0.2, -1.0)


-- ================================= LEGS ===========================================
-- create leg
-- rotY: rotation about y axis. 
-- rotZ: rotation about z axis
function create_leg(name, tx, ty, tz, rotY, rotZ)
    leg_length = 1.0

    -- Create a joint at the leg's attachment point.
    leg_joint = gr.joint(name .. "_joint", {rotZ - 8, 0, rotZ + 8}, {0, 0, 0})
    leg_joint:translate(tx, ty, tz)
    leg_joint:rotate('y', rotY)
    leg_joint:rotate('z', rotZ)
    torso_joint:add_child(leg_joint)

    -- create the upper leg geometry node 
    upper_leg = gr.mesh('cube', name .. "_upper")
    upper_leg:set_material(gr.material({0.05, 0.05, 0.05}, {0.8, 0.8, 0.8}, 100))
    upper_leg:scale(leg_length, 0.2, 0.2)   -- long and thin.
    leg_joint:add_child(upper_leg)

    knee_pivot = gr.joint(name .. "_knee_pivot", {0, 0, 0}, {0, 0, 0})
    if tx > 0 then
        knee_pivot:translate((leg_length / 2), 0.0, 0.0)
    else
        knee_pivot:translate(-(leg_length / 2), 0.0, 0.0)
    end
    leg_joint:add_child(knee_pivot)
    
    
    -- add a knee joint for the lower leg.
    local z_limit = {0, 0, 0}
    if tx > 0 then
        z_limit = {-50, 0, 20}
    else
        z_limit = {-20, 0, 50}
    end
    knee_joint = gr.joint(name .. "_knee", z_limit, {-30, 0, 30} )
    if tx > 0 then
        knee_joint:translate(leg_length / 2, 0, 0)          
    else
        knee_joint:translate(-leg_length / 2, 0, 0)         
    end
    knee_pivot:add_child(knee_joint)

    -- create the lower leg segment geometric node
    lower_leg = gr.mesh('cube', name .. "_lower")
    knee_joint:add_child(lower_leg)
    lower_leg:scale(leg_length, 0.15, 0.15)    -- even thinner.
end

local y_leg_offset = -0.2
local x_leg_offset = 0.9
local leg_lift = 25

-- Create legs on the right side.
-- These legs attach on the right side of the body (positive x).
create_leg("leg1",  x_leg_offset - 0.3, y_leg_offset,  0.1,  45, -leg_lift)
create_leg("leg2",  x_leg_offset, y_leg_offset,  0.1,  25, -leg_lift)
create_leg("leg3",  x_leg_offset, y_leg_offset, -0.1, -25, -leg_lift)
create_leg("leg4",  x_leg_offset - 0.3, y_leg_offset, -0.1, -45, -leg_lift)

-- Create legs on the left side.
-- For the left side, attach with negative x and mirror the rotation.
create_leg("leg5", -x_leg_offset + 0.3, y_leg_offset,  0.1, -45, leg_lift)
create_leg("leg6", -x_leg_offset, y_leg_offset,  0.1, -25, leg_lift)
create_leg("leg7", -x_leg_offset, y_leg_offset, -0.1,  25, leg_lift)
create_leg("leg8", -x_leg_offset + 0.3, y_leg_offset, -0.1,  45, leg_lift)

root:translate(0.0, -1.0, -8.0)
return root
