#include "diffusion_grid.h"
#include "cell.h"
#include "grid.h"
#include "gtest/gtest.h"
#include "unit/test_util.h"

namespace bdm {

template <typename TContainer>
void CellFactory(TContainer* cells, std::vector<std::array<double, 3>>& positions) {
  cells->reserve(positions.size());
  for (size_t i = 0; i < positions.size(); i++) {
    Cell cell({positions[i][0], positions[i][1], positions[i][2]});
    cell.SetDiameter(30);
    cells->push_back(cell);
  }
}

// Test if the dimensions of the diffusion grid are corresponding to the
// neighbor grid dimensions
TEST(DiffusionTest, GridDimensions) {
  auto rm = ResourceManager<>::Get();
  rm->Clear();
  auto cells = rm->Get<Cell>();
  
  std::vector<std::array<double, 3>> positions;
  positions.push_back({-10, -10, -10});
  positions.push_back({90, 90, 90});
  CellFactory(cells, positions);

  DiffusionGrid* d_grid = new DiffusionGrid("Kalium", 0.4);

  auto& grid = Grid<>::GetInstance();
  grid.Initialize();
  d_grid->Initialize(grid.GetDimensions(), grid.GetBoxLength());

  auto dims = d_grid->GetDimensions();

  EXPECT_EQ(-40, dims[0]);
  EXPECT_EQ(-40, dims[2]);
  EXPECT_EQ(-40, dims[4]);
  EXPECT_EQ(140, dims[1]);
  EXPECT_EQ(140, dims[3]);
  EXPECT_EQ(140, dims[5]);

  delete d_grid;
}

// Test if the dimension of the diffusion grid update correctly with the
// neighbor grid dimensions (we expect the diffusion grid to stay cube-shaped)
TEST(DiffusionTest, UpdateGrid) {
  auto rm = ResourceManager<>::Get();
  rm->Clear();
  auto cells = rm->Get<Cell>();
  
  std::vector<std::array<double, 3>> positions;
  positions.push_back({-10, -10, -10});
  positions.push_back({90, 90, 90});
  CellFactory(cells, positions);

  DiffusionGrid* d_grid = new DiffusionGrid("Kalium", 0.4);

  auto& grid = Grid<>::GetInstance();
  grid.Initialize();
  d_grid->Initialize(grid.GetDimensions(), grid.GetBoxLength());

  std::vector<std::array<double, 3>> positions_2;
  positions_2.push_back({-30, -10, -10});
  positions_2.push_back({90, 150, 90});
  CellFactory(cells, positions_2);

  grid.UpdateGrid();

  d_grid->Update(grid.GetDimensions());

  auto d_dims = d_grid->GetDimensions();

  EXPECT_EQ(-60, d_dims[0]);
  EXPECT_EQ(-60, d_dims[2]);
  EXPECT_EQ(-60, d_dims[4]);
  EXPECT_EQ(210, d_dims[1]);
  EXPECT_EQ(210, d_dims[3]);
  EXPECT_EQ(210, d_dims[5]);

  delete d_grid;
}

// Test if the diffusion grid does not change if the neighbor grid dimensions
// do not change
TEST(DiffusionTest, FalseUpdateGrid) {
  auto rm = ResourceManager<>::Get();
  rm->Clear();
  auto cells = rm->Get<Cell>();
  
  std::vector<std::array<double, 3>> positions;
  positions.push_back({-10, -10, -10});
  positions.push_back({90, 90, 90});
  CellFactory(cells, positions);

  DiffusionGrid* d_grid = new DiffusionGrid("Kalium", 0.4);

  auto& grid = Grid<>::GetInstance();
  grid.Initialize();
  d_grid->Initialize(grid.GetDimensions(), grid.GetBoxLength());
  d_grid->Update(grid.GetDimensions());

  auto dims = d_grid->GetDimensions();

  EXPECT_EQ(-40, dims[0]);
  EXPECT_EQ(-40, dims[2]);
  EXPECT_EQ(-40, dims[4]);
  EXPECT_EQ(140, dims[1]);
  EXPECT_EQ(140, dims[3]);
  EXPECT_EQ(140, dims[5]);

  d_grid->Update(grid.GetDimensions());

  dims = d_grid->GetDimensions();

  EXPECT_EQ(-40, dims[0]);
  EXPECT_EQ(-40, dims[2]);
  EXPECT_EQ(-40, dims[4]);
  EXPECT_EQ(140, dims[1]);
  EXPECT_EQ(140, dims[3]);
  EXPECT_EQ(140, dims[5]);

  delete d_grid;
}

// Create a 5x5x5 diffusion grid, with a substance being
// added at center box 2,2,2, causing a symmetrical diffusion
TEST(DiffusionTest, LeakingEdge) {
  auto rm = ResourceManager<>::Get();
  rm->Clear();
  auto cells = rm->Get<Cell>();
  
  std::vector<std::array<double, 3>> positions;
  positions.push_back({0, 0, 0});
  positions.push_back({60, 60, 60});
  CellFactory(cells, positions);

  DiffusionGrid* d_grid = new DiffusionGrid("Kalium", 0.4);

  auto& grid = Grid<>::GetInstance();
  grid.Initialize();
  d_grid->Initialize(grid.GetDimensions(), grid.GetBoxLength());
  d_grid->SetConcentrationThreshold(1e15);

  for (int i = 0; i < 100; i ++) {
    d_grid->IncreaseConcentrationBy({{45,45,45}}, 4);
    d_grid->Update(grid.GetDimensions());
    d_grid->RunDiffusionStep();
    d_grid->CalculateGradient();
  }

  // Get concentrations and gradients after 100 time steps
  auto conc = d_grid->GetAllConcentrations();
  auto grad = d_grid->GetAllGradients();

  array<uint32_t, 3> c = {2,2,2};
  array<uint32_t, 3> w = {1,2,2};
  array<uint32_t, 3> e = {3,2,2};
  array<uint32_t, 3> n = {2,1,2};
  array<uint32_t, 3> s = {2,3,2};
  array<uint32_t, 3> t = {2,2,1};
  array<uint32_t, 3> b = {2,2,3};
  array<uint32_t, 3> rand1_a = {0,0,0};
  array<uint32_t, 3> rand1_b = {4,4,4};
  array<uint32_t, 3> rand2_a = {4,4,2};
  array<uint32_t, 3> rand2_b = {0,0,2};

  EXPECT_DOUBLE_EQ(9.7267657389657938, conc[d_grid->GetBoxIndex(c)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(e)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(w)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(n)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(s)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(t)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(b)]);
  EXPECT_DOUBLE_EQ(0.12493663388071227, conc[d_grid->GetBoxIndex(rand1_a)]);
  EXPECT_DOUBLE_EQ(0.12493663388071227, conc[d_grid->GetBoxIndex(rand1_b)]);
  EXPECT_DOUBLE_EQ(0.32563083857294983, conc[d_grid->GetBoxIndex(rand2_a)]);
  EXPECT_DOUBLE_EQ(0.32563083857294983, conc[d_grid->GetBoxIndex(rand2_b)]);

  EXPECT_DOUBLE_EQ(0.0000000000000000,  grad[3*(d_grid->GetBoxIndex(c))+1]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(e))+0]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(w))+0]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(n))+1]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(s))+1]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(t))+2]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(b))+2]);
  EXPECT_DOUBLE_EQ(0.0033449034115372936, grad[3*(d_grid->GetBoxIndex(rand1_a))+1]);
  EXPECT_DOUBLE_EQ(-0.0033449034115372936, grad[3*(d_grid->GetBoxIndex(rand1_b))+1]);
  EXPECT_DOUBLE_EQ(-0.013002938053771644, grad[3*(d_grid->GetBoxIndex(rand2_a))+0]);
  EXPECT_DOUBLE_EQ(0.013002938053771644, grad[3*(d_grid->GetBoxIndex(rand2_b))+0]);

  delete d_grid;
}

// Tests if the concentration / gradient values are correctly copied
// after the grid has grown and DiffusionGrid::CopyOldData is called
TEST(DiffusionTest, CopyOldData) {
  auto rm = ResourceManager<>::Get();
  rm->Clear();
  auto cells = rm->Get<Cell>();
  
  std::vector<std::array<double, 3>> positions;
  positions.push_back({0, 0, 0});
  positions.push_back({60, 60, 60});
  CellFactory(cells, positions);

  DiffusionGrid* d_grid = new DiffusionGrid("Kalium", 0.4);

  auto& grid = Grid<>::GetInstance();
  grid.Initialize();
  d_grid->Initialize(grid.GetDimensions(), grid.GetBoxLength());
  d_grid->SetConcentrationThreshold(1e15);

  for (int i = 0; i < 100; i ++) {
    d_grid->IncreaseConcentrationBy({{45,45,45}}, 4);
    d_grid->Update(grid.GetDimensions());
    d_grid->RunDiffusionStep();
    d_grid->CalculateGradient();
  }

  // Increase the diffusion grid to a 7x7x7
  std::vector<std::array<double, 3>> positions_2;
  positions_2.push_back({90, 60, 60});
  CellFactory(cells, positions_2);

  grid.UpdateGrid();
  d_grid->Update(grid.GetDimensions());

  // Get concentrations and gradients after 100 time steps
  auto conc = d_grid->GetAllConcentrations();
  auto grad = d_grid->GetAllGradients();

  array<uint32_t, 3> c = {3,3,3};
  array<uint32_t, 3> w = {2,3,3};
  array<uint32_t, 3> e = {4,3,3};
  array<uint32_t, 3> n = {3,2,3};
  array<uint32_t, 3> s = {3,4,3};
  array<uint32_t, 3> t = {3,3,2};
  array<uint32_t, 3> b = {3,3,4};
  array<uint32_t, 3> rand1_a = {1,1,1};
  array<uint32_t, 3> rand1_b = {5,5,5};
  array<uint32_t, 3> rand2_a = {5,5,3};
  array<uint32_t, 3> rand2_b = {1,1,3};

  EXPECT_DOUBLE_EQ(9.7267657389657938, conc[d_grid->GetBoxIndex(c)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(e)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(w)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(n)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(s)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(t)]);
  EXPECT_DOUBLE_EQ(3.7281869469803648, conc[d_grid->GetBoxIndex(b)]);
  EXPECT_DOUBLE_EQ(0.12493663388071227, conc[d_grid->GetBoxIndex(rand1_a)]);
  EXPECT_DOUBLE_EQ(0.12493663388071227, conc[d_grid->GetBoxIndex(rand1_b)]);
  EXPECT_DOUBLE_EQ(0.32563083857294983, conc[d_grid->GetBoxIndex(rand2_a)]);
  EXPECT_DOUBLE_EQ(0.32563083857294983, conc[d_grid->GetBoxIndex(rand2_b)]);

  EXPECT_DOUBLE_EQ(0.0000000000000000,  grad[3*(d_grid->GetBoxIndex(c))+1]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(e))+0]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(w))+0]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(n))+1]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(s))+1]);
  EXPECT_DOUBLE_EQ(0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(t))+2]);
  EXPECT_DOUBLE_EQ(-0.14368264361944241,  grad[3*(d_grid->GetBoxIndex(b))+2]);
  EXPECT_DOUBLE_EQ(0.0033449034115372936, grad[3*(d_grid->GetBoxIndex(rand1_a))+1]);
  EXPECT_DOUBLE_EQ(-0.0033449034115372936, grad[3*(d_grid->GetBoxIndex(rand1_b))+1]);
  EXPECT_DOUBLE_EQ(-0.013002938053771644, grad[3*(d_grid->GetBoxIndex(rand2_a))+0]);
  EXPECT_DOUBLE_EQ(0.013002938053771644, grad[3*(d_grid->GetBoxIndex(rand2_b))+0]);

  delete d_grid;
}

}  // namespace bdm
