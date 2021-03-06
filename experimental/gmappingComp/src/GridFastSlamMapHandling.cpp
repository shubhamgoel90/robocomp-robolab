/*
* @file GridFastSlamMapHandling.h
* This file implements static functions for loading and saving maps generated by GMapping.
* @author <a href="mailto:Tim.Laue@dfki.de">Tim Laue</a>
*/

#include "GridFastSlamMapHandling.h"
#include <iostream>

using namespace GMapping;


void GridFastSlamMapHandling::saveMap(const GMapping::ScanMatcherMap& map, const std::string& mapName)
{
  // Open file:
  FILE* mapFile = 0;
  mapFile = fopen(mapName.c_str(), "wb+" );
  if(!mapFile) // In case of error
  {
    std::cout << mapName << " cannot be opened.\n";
    return;
  }

  // Save relevant members of Map class:
  Point center = map.getCenter();
  double delta = map.getDelta();
  double xmin, ymin, xmax, ymax;
  map.getSize(xmin, ymin, xmax, ymax);
  fwrite(&center, sizeof(double), 2, mapFile);
  fwrite(&delta,  sizeof(double), 1, mapFile);
  fwrite(&xmin,   sizeof(double), 1, mapFile);
  fwrite(&ymin,   sizeof(double), 1, mapFile);
  fwrite(&xmax,   sizeof(double), 1, mapFile);
  fwrite(&ymax,   sizeof(double), 1, mapFile);

  // Save members of HierarchicalArray2D class:
  const HierarchicalArray2D<PointAccumulator>::PointSet& activeArea = map.storage().getActiveArea();
  HierarchicalArray2D<PointAccumulator>::PointSet::const_iterator iter;
  int numOfPoints(activeArea.size());
  fwrite(&numOfPoints, sizeof(int), 1, mapFile);
  for(iter = activeArea.begin(); iter != activeArea.end(); ++iter)
  {
    point<int> p = *iter;
    fwrite(&p, sizeof(int), 2, mapFile);
  }
  int xSize = map.storage().getXSize();
  int ySize = map.storage().getYSize();
  // Determine number of active cells before and save it to file:
  int numberOfActiveHCells(0);
  for(int x=0; x<xSize; x++)
    for(int y=0; y<ySize; y++)
      if(map.storage().m_cells[x][y])
        ++numberOfActiveHCells;
  fwrite(&numberOfActiveHCells, sizeof(int), 1, mapFile);
  // Loop through all cells and save local arrays:
  for(int x=0; x<xSize; x++)
  {
    for(int y=0; y<ySize; y++)
    {
      autoptr< Array2D<PointAccumulator> >& localArray = map.storage().m_cells[x][y];
      if(localArray)
      {
        // Save coordinates:
        fwrite(&x, sizeof(int), 1, mapFile);
        fwrite(&y, sizeof(int), 1, mapFile);
        int localXSize = localArray.m_reference->data->getXSize();
        int localYSize = localArray.m_reference->data->getYSize();
        // Determine number of active cells before and save it to file:
        int numberOfActiveCells(0);
        for(int xx=0; xx<localXSize; xx++)
          for(int yy=0; yy<localYSize; yy++)
            if(localArray.m_reference->data->m_cells[xx][yy] != -1)
              ++numberOfActiveCells;
        fwrite(&numberOfActiveCells, sizeof(int), 1, mapFile);
        // Now save contents of all active local cells:
        for(int xx=0; xx<localXSize; xx++)
        {
          for(int yy=0; yy<localYSize; yy++)
          {
            if(localArray.m_reference->data->m_cells[xx][yy] != -1)
            {
              // Save coordinates:
              fwrite(&xx, sizeof(int), 1, mapFile);
              fwrite(&yy, sizeof(int), 1, mapFile);
              PointAccumulator& pAcc = localArray.m_reference->data->m_cells[xx][yy];
              fwrite(&pAcc, sizeof(PointAccumulator), 1, mapFile);
            }
          }
        }
      }
    }
  }
  // Close file:
  fclose(mapFile);
}


GMapping::ScanMatcherMap* GridFastSlamMapHandling::loadMap(const std::string& mapName)
{
   // Open file:
  FILE* mapFile = 0;
  mapFile = fopen(mapName.c_str(), "rb" );
  if(!mapFile) // In case of error
  {
    std::cout << mapName << " cannot be opened.\n";
    return 0;
  }

  // Create Map object:
  Point center;
  double delta;
  double xmin, ymin, xmax, ymax;
  fread(&center, sizeof(double), 2, mapFile);
  fread(&delta, sizeof(double), 1, mapFile);
  fread(&xmin, sizeof(double), 1, mapFile);
  fread(&ymin, sizeof(double), 1, mapFile);
  fread(&xmax, sizeof(double), 1, mapFile);
  fread(&ymax, sizeof(double), 1, mapFile);
  printf("center(%g,%g) delta(%g) xmin(%g) xmax(%g) ymin(%g) ymax(%g)\n", center.x, center.y, delta, xmin, xmax, ymin, ymax);
  ScanMatcherMap* map = new ScanMatcherMap(center, xmin, ymin, xmax, ymax, delta);

  // Read members of HierarchicalArray2D class:
  int numberOfActiveAreas;
  fread(&numberOfActiveAreas, sizeof(int), 1, mapFile);
  HierarchicalArray2D<PointAccumulator>::PointSet activeAreas;
  for(int i=0; i<numberOfActiveAreas; i++)
  {
    point<int> p;
    fread(&p, sizeof(int), 2, mapFile);
    activeAreas.insert(p);
  }
  map->storage().setActiveArea(activeAreas);
  int numberOfActiveHCells;
  fread(&numberOfActiveHCells, sizeof(int), 1, mapFile);
  
  printf("numberOfActiveHCells %d\n", numberOfActiveHCells);
  for(int i=0; i<numberOfActiveHCells; i++)
  {
    //Get position in hierarchical grid and the number of inner cells:
    int x,y,numberOfActiveCells;
    fread(&x, sizeof(int), 1, mapFile);
    fread(&y, sizeof(int), 1, mapFile);
    fread(&numberOfActiveCells, sizeof(int), 1, mapFile);
    int patchMagnitude = map->storage().getPatchMagnitude();
	printf("< %d\n", i);
    Array2D<PointAccumulator>* patch = new Array2D<PointAccumulator>(1<<patchMagnitude, 1<<patchMagnitude);
	map->storage().m_cells[x][y] = autoptr< Array2D<PointAccumulator> >(patch);
	printf(">\n");
    autoptr< Array2D<PointAccumulator> >& localArray = map->storage().m_cells[x][y];
    for(int j=0; j<numberOfActiveCells; j++)
    {
      int xx,yy;
      fread(&xx, sizeof(int), 1, mapFile);
      fread(&yy, sizeof(int), 1, mapFile);
      PointAccumulator& pAcc = localArray.m_reference->data->m_cells[xx][yy];
      fread(&pAcc, sizeof(PointAccumulator), 1, mapFile);
    }
  }
  // Close file and return map:
  fclose(mapFile);
  return map;
}
