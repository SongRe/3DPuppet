-- minecraft_spider.lua
-- A simple Minecraft-style spider model

-- Create the root node for the spider.
root = gr.node('root')

spider = gr.node('spider')
root:add_child(spider)

-- Create the body. We'll use a cube, then scale it to a rectangular prism.
body = gr.mesh('cube', 'body')
body:set_material(gr.material({0.3, 0.3, 0.3}, {0.8, 0.8, 0.8}, 50))
body:scale(2.0, 1.0, 3.0)
spider:add_child(body)

-- Helper function to create a leg.
-- 'name' is the leg's name and 'pos' is a 3-element table giving the leg joint position relative to the body.
function create_leg(name, pos)
    -- Create a base node to position the leg relative to the body.
    local leg_base = gr.node(name .. "_base")
    leg_base:translate(pos[1], pos[2], pos[3])
    
    -- Create a joint node for the leg.
    -- Here, we allow the leg to rotate around its local x-axis (swing up/down).
    -- Adjust the angle limits as needed.
    local leg_joint = gr.joint(name .. "_joint", {-45, 0, 45}, {0, 0, 0})
    leg_base:add_child(leg_joint)
    
    -- Create the leg geometry using a cube.
    local leg = gr.mesh('cube', name)
    leg:set_material(gr.material({0.1, 0.1, 0.1}, {0.8, 0.8, 0.8}, 10))
    -- Scale the cube to form a thin, elongated leg.
    leg:scale(0.1, 0.1, 1.0)
    -- Translate so the leg geometry extends outward.
    leg:translate(0, 0, -0.5)
    leg_joint:add_child(leg)
    
    return leg_base
end

-- Define positions for legs relative to the body.
-- The body is scaled to 2.0 (width) x 1.0 (height) x 3.0 (length) and centered at the origin.
-- We'll attach 4 legs to each side.
local left_positions = {
    {-1.0, 0,  1.2},  -- front left
    {-1.0, 0,  0.4},  -- mid-front left
    {-1.0, 0, -0.4},  -- mid-back left
    {-1.0, 0, -1.2}   -- back left
}

local right_positions = {
    {1.0, 0,  1.2},   -- front right
    {1.0, 0,  0.4},   -- mid-front right
    {1.0, 0, -0.4},   -- mid-back right
    {1.0, 0, -1.2}    -- back right
}

-- Create and attach left-side legs.
for i, pos in ipairs(left_positions) do
    local leg = create_leg("leg" .. i, pos)
    body:add_child(leg)
end

-- Create and attach right-side legs.
for i, pos in ipairs(right_positions) do
    local leg = create_leg("leg" .. (i+4), pos)
    -- For the right side, mirror the leg along the x-axis.
    leg:scale(-1.0, 1.0, 1.0)
    body:add_child(leg)
end

return spider
