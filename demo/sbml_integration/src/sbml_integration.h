// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------
#ifndef SBML_INTEGRATION_H_
#define SBML_INTEGRATION_H_

#include "biodynamo.h"
#include "core/util/io.h"
#include "core/util/timing.h"

#include <TAxis.h>
#include <TCanvas.h>
#include <TFrame.h>
#include <TGraph.h>
#include <TMultiGraph.h>
#include <TPad.h>

#include "rrException.h"
#include "rrExecutableModel.h"
#include "rrLogger.h"
#include "rrRoadRunner.h"
#include "rrUtils.h"

namespace bdm {

// Define my custom cell, which extends Cell by adding an extra
// data member s1_.
class MyCell : public Cell {
  BDM_AGENT_HEADER(MyCell, Cell, 1);

 public:
  MyCell() {}
  explicit MyCell(const Double3& position) : Base(position) {}
  virtual ~MyCell() {}

  void SetS1(double s1) { s1_ = s1; }
  int GetS1() const { return s1_; }

 private:
  double s1_ = 100;
};

// Define SbmlBehavior to simulate intracellular chemical reaction network.
class SbmlBehavior : public Behavior {
  BDM_BEHAVIOR_HEADER(SbmlBehavior, Behavior, 1)

 public:
  SbmlBehavior() {}
  SbmlBehavior(const std::string& sbml_file, const rr::SimulateOptions& opt) {
    Initialize(sbml_file, opt);
  }

  SbmlBehavior(const SbmlBehavior& other) {
    auto other_sbml_behavior = bdm_static_cast<const SbmlBehavior*>(&other);
    Initialize(other_sbml_behavior->sbml_file_,
               other_sbml_behavior->initial_options_);
    result_ = other_sbml_behavior->result_;
  }

  virtual ~SbmlBehavior() { delete rr_; }

  void Initialize(const std::string& sbml_file,
                  const rr::SimulateOptions& opt) {
    sbml_file_ = sbml_file;
    initial_options_ = opt;

    rr_ = new rr::RoadRunner(sbml_file);
    rr_->getSimulateOptions() = opt;
    // setup integrator
    rr_->setIntegrator("gillespie");
    dt_ = opt.duration / opt.steps;
    auto* integrator = rr_->getIntegrator();
    integrator->setValue("variable_step_size", false);
    integrator->setValue("initial_time_step", dt_);
    integrator->setValue("maximum_time_step", dt_);

    result_.resize(opt.steps, 4);
  }

  void Run(Agent* agent) override {
    if (auto* cell = static_cast<MyCell*>(agent)) {
      auto i = Simulation::GetActive()->GetScheduler()->GetSimulatedSteps();
      rr_->getIntegrator()->integrate(i * dt_, dt_);
      // FIXME model time not the same as
      const auto& partial_result = rr_->getFloatingSpeciesAmountsNamedArray();
      cell->SetS1(partial_result(0, 0));
      result_(i, 0) = i * dt_;
      for (unsigned j = 0; j < partial_result.numCols(); j++) {
        result_(i, j + 1) = partial_result(0, j);
      }

      if (cell->GetS1() < 30 && active_) {
        cell->Divide();
        active_ = false;
      }
    }
  }

  const ls::DoubleMatrix& GetResult() const { return result_; }

 private:
  std::string sbml_file_;
  rr::SimulateOptions initial_options_;
  ls::DoubleMatrix result_;
  bool active_ = true;
  rr::RoadRunner* rr_;
  double dt_;
};

inline void AddToPlot(TMultiGraph* mg, const ls::Matrix<double>* result) {
  ls::Matrix<double> foo1(*result);
  ls::Matrix<double> foo(*foo1.getTranspose());
  int rows;
  int cols;
  auto** twod = foo.get2DMatrix(rows, cols);

  TGraph* gr = new TGraph(cols, twod[0], twod[1]);
  gr->SetFillStyle(0);
  gr->SetLineColorAlpha(2, 0.1);
  gr->SetLineWidth(1);
  gr->SetTitle("S1");

  TGraph* gr1 = new TGraph(cols, twod[0], twod[2]);
  gr1->SetTitle("S2");
  gr1->SetLineColorAlpha(3, 0.1);
  gr1->SetLineWidth(1);

  TGraph* gr2 = new TGraph(cols, twod[0], twod[3]);
  gr2->SetTitle("S3");
  gr2->SetLineColorAlpha(4, 0.1);
  gr2->SetLineWidth(1);

  mg->Add(gr);
  mg->Add(gr1);
  mg->Add(gr2);
  mg->Draw("AL C C");
}

inline void PlotSbmlBehaviors(const char* filename) {
  // setup plot
  TCanvas c;
  c.SetGrid();

  TMultiGraph* mg = new TMultiGraph();
  mg->SetTitle("Gillespie;Timestep;Concentration");

  Simulation::GetActive()->GetResourceManager()->ForEachAgent(
      [&](Agent* agent) {
        auto* cell = static_cast<MyCell*>(agent);
        const auto& behaviour = cell->GetAllBehaviors();
        if (behaviour.size() == 1) {
          AddToPlot(mg, &static_cast<SbmlBehavior*>(behaviour[0])->GetResult());
        }
      });

  // finalize plot
  // TCanvas::Update() draws the frame, after which one can change it
  c.Update();
  c.GetFrame()->SetBorderSize(12);
  gPad->Modified();
  gPad->Update();
  c.Modified();
  c.cd(0);
  // c.BuildLegend(); // TODO position of legend
  c.SaveAs(filename);
}

inline int Simulate(int argc, const char** argv) {
  auto opts = CommandLineOptions(argc, argv);
  opts.AddOption<uint64_t>("n, num-cells", "10", "The total number of cells");
  uint64_t num_cells = opts.Get<uint64_t>("num-cells");

  // roadrunner options
  rr::SimulateOptions opt;
  opt.start = 0;
  opt.duration = 10;
  opt.steps = 100;

  auto set_param = [&](Param* param) {
    param->simulation_time_step = opt.duration / opt.steps;
  };

  Simulation simulation(&opts, set_param);

  std::string sbml_file = "../src/sbml-model.xml";
  if (!FileExists(sbml_file)) {
    sbml_file = "src/sbml-model.xml";
    if (!FileExists(sbml_file)) {
      Log::Error("Could not find sbml_model.xml file.");
    }
  }

  // Define initial model
  auto construct = [&](const Double3& position) {
    auto* cell = new MyCell();
    cell->SetPosition(position);
    cell->SetDiameter(10);
    cell->AddBehavior(new SbmlBehavior(sbml_file, opt));
    return cell;
  };
  ModelInitializer::CreateAgentsRandom(0, 200, num_cells, construct);

  // Run simulation
  auto start = Timing::Timestamp();
  simulation.GetScheduler()->Simulate(opt.steps);
  auto stop = Timing::Timestamp();
  std::cout << "RUNTIME " << (stop - start) << std::endl;

  PlotSbmlBehaviors("sbml-behaviors.svg");

  std::cout << "Simulation completed successfully!" << std::endl;
  return 0;
}

}  // namespace bdm

#endif  // SBML_INTEGRATION_H_
