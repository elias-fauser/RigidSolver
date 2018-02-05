particlePosition = (0.0, 0.5, 0.0)
btmLeftFrontCorner = (-0.5, -0.5, -0.5)
voxelLength = 0.01
gridSize = (1.0, 1.0)

normalizedX = (int((particlePosition[0] - btmLeftFrontCorner[0]) / voxelLength) * voxelLength) / gridSize[0];
normalizedY = (int((particlePosition[1] - btmLeftFrontCorner[1]) / voxelLength) * voxelLength) / gridSize[1];

print normalizedX
print normalizedY