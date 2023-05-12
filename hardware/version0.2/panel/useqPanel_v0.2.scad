

panelThickness = 2.8;
panelHp=8;
holeCount=4;
holeWidth = 5.08; 


threeUHeight = 133.35; //overall 3u height
panelOuterHeight =128.5;
panelInnerHeight = 110; //rail clearance = ~11.675mm, top and bottom
railHeight = (threeUHeight-panelOuterHeight)/2;
mountSurfaceHeight = (panelOuterHeight-panelInnerHeight-railHeight*2)/2;

echo("mountSurfaceHeight",mountSurfaceHeight);

hp=5.08;
mountHoleDiameter = 3.2;
mountHoleRad =mountHoleDiameter/2;
hwCubeWidth = holeWidth-mountHoleDiameter;

offsetToMountHoleCenterY=mountSurfaceHeight/2;
offsetToMountHoleCenterX = hp - hwCubeWidth/2; // 1 hp from side to center of hole

echo(offsetToMountHoleCenterY);
echo(offsetToMountHoleCenterX);

module eurorackPanel(panelHp,  mountHoles=2, hw = holeWidth, ignoreMountHoles=false)
{
    //mountHoles ought to be even. Odd values are -=1
    difference()
    {
        cube([hp*panelHp,panelOuterHeight,panelThickness]);
        
        if(!ignoreMountHoles)
        {
            eurorackMountHoles(panelHp, mountHoles, holeWidth);
        }
    }
}

module eurorackMountHoles(php, holes, hw)
{
    holes = holes-holes%2;
    eurorackMountHolesTopRow(php, hw, holes/2);
    eurorackMountHolesBottomRow(php, hw, holes/2);
}

module eurorackMountHolesTopRow(php, hw, holes)
{
    
    //topleft
    translate([offsetToMountHoleCenterX,panelOuterHeight-offsetToMountHoleCenterY,0])
    {
        eurorackMountHole(hw);
    }
    if(holes>1)
    {
        translate([(hp*php)-hwCubeWidth-hp,panelOuterHeight-offsetToMountHoleCenterY,0])
    {
        eurorackMountHole(hw);
    }
    }
    if(holes>2)
    {
        holeDivs = php*hp/(holes-1);
        for (i =[1:holes-2])
        {
            translate([holeDivs*i,panelOuterHeight-offsetToMountHoleCenterY,0]){
                eurorackMountHole(hw);
            }
        }
    }
}

module eurorackMountHolesBottomRow(php, hw, holes)
{
    
    //bottomRight
    translate([(hp*php)-hwCubeWidth-hp,offsetToMountHoleCenterY,0])
    {
        eurorackMountHole(hw);
    }
    if(holes>1)
    {
        translate([offsetToMountHoleCenterX,offsetToMountHoleCenterY,0])
    {
        eurorackMountHole(hw);
    }
    }
    if(holes>2)
    {
        holeDivs = php*hp/(holes-1);
        for (i =[1:holes-2])
        {
            translate([holeDivs*i,offsetToMountHoleCenterY,0]){
                eurorackMountHole(hw);
            }
        }
    }
}

module eurorackMountHole(hw)
{
    
    mountHoleDepth = panelThickness+2; //because diffs need to be larger than the object they are being diffed from for ideal BSP operations
    
    if(hwCubeWidth<0)
    {
        hwCubeWidth=0;
    }
    translate([0,0,-1]){
    union()
    {
        cylinder(r=mountHoleRad, h=mountHoleDepth, $fn=20);
        translate([0,-mountHoleRad,0]){
        cube([hwCubeWidth, mountHoleDiameter, mountHoleDepth]);
        }
        translate([hwCubeWidth,0,0]){
            cylinder(r=mountHoleRad, h=mountHoleDepth, $fn=20);
            }
    }
}
}











///////////////////////////////

w = hp*panelHp;
ledholeRadius =2.55;

module hollowCylinder(ch, cr1, cr2, th) {
    difference(){
    cylinder(10, cr1, cr2, center=true, $fn=128);
    cylinder(10, cr1-th, cr2-th, center=true, $fn=128);
    }
}


module drawPanelWithHoles() {
    rSocket=3.5;
    rRotaryEnc=4;
    rSwitch=3.5;
    rMomentarySwitch=4;
    rLed = 2;
    rReset = 1.5;
    difference() {
        eurorackPanel(panelHp, holeCount,holeWidth);

        base = ((panelOuterHeight -panelInnerHeight) / 2) +1;

        translate([(w-28.7), base+16.5  , 0])
        cylinder(10, rMomentarySwitch,rMomentarySwitch, center=true, $fn=128);
        translate([(w-28.7), base+44.45 , 0])
        cylinder(10, rMomentarySwitch,rMomentarySwitch, center=true, $fn=128);

        translate([w-28.7, base+30.5, 0])
        cylinder(10, rSwitch,rSwitch, center=true, $fn=128);
        translate([(w-7.76), base + 56.41, 0])
        cylinder(10, rSwitch,rSwitch, center=true, $fn=128);

        //rotary encoder
        translate([(w-8.69), base+38.2, 0])
        cylinder(10, rRotaryEnc, rRotaryEnc, center=true, $fn=128);

        //reset hole
        translate([(w-31.6), base+2.8, 0])
        cylinder(10, rReset, rReset, center=true, $fn=128);

        //USB socket
        translate([(w-20.14), base+5.62, 0])
        cylinder(10, rSocket,rSocket, center=true, $fn=128);

        for(h=[70.9, 81.45, 91.93, 102.58]) {
            
            //sockets
            translate([(w-6.73), base + h, 0])
            cylinder(10, rSocket,rSocket, center=true, $fn=128);
            
            translate([(w - 33.9), base + h, 0])
            cylinder(10, rSocket,rSocket, center=true, $fn=128);
            
            //LEDs
            translate([(w-17.68), base + h-2.76, 0])
            cylinder(10, rLed, rLed, center=true, $fn=128);
            translate([(w-22.94), base + h + 2.74, 0])
            cylinder(10, rLed,rLed, center=true, $fn=128);
            

        }

        translate([w * 0.77, 8, 0.5]) rotate([0,0,180])
            linear_extrude(4)
                text("uSEQ", size = 7,font="Lato");
        translate([w * .93, 18, 0.5]) rotate([0,0,180])
            linear_extrude(4)
                text("usb", size = 5,font="Lato");
        for(x=[.7]) {
            translate([w * x, 85, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↤", size = 5,font="De­jaVu");
            translate([w * x, 95, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("∏∐", size = 3.5,font="De­jaVu",spacing=0.7);
            translate([w * x,108, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↦", size = 5,font="De­jaVu");
            translate([w * x,119, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↦", size = 5,font="De­jaVu");
        }
        for(x=[.45]) {
            os=-4.7;
            translate([w * x, 85+os, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↤", size = 5,font="De­jaVu");
            translate([w * x, 95+os, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("∏∐", size = 3.5,font="De­jaVu",spacing=0.7);
            translate([w * x,108+os, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↦", size = 5,font="De­jaVu");
            translate([w * x,119+os, 0.5]) rotate([0,0,180])
                linear_extrude(4)
                    text("↦", size = 5,font="De­jaVu");
        }
    }
    
}

drawPanelWithHoles();


