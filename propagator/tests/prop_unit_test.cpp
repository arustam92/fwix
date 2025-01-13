#include <complex4DReg.h>
#include "PhaseShift.h"
#include <gtest/gtest.h>
#include <complex_vector.h>
#include <cuda_runtime.h>
#include  <RefSampler.h>
#include <Selector.h>
#include <OneStep.h>
#include <Injection.h>
#include <OneWay.h>

#include <jsonParamObj.h>
#include <random>

bool verbose = false;
double tolerance = 1e-5;

class PS_Test : public testing::Test {
 protected:
  void SetUp() override {
    n1 = 100;
    n2 = 100;
    n3 = 10;
    n4 = 10;

    // create a vector of slowness values for each frequency
    std::vector<std::complex<float>> slow(n3, {1.f, 0.f});
    auto hyper = std::make_shared<hypercube>(n1, n2, n3, n4);
    space4d = std::make_shared<complex4DReg>(hyper);
    space4d->set(1.f);
    ps = std::make_unique<PhaseShift>(hyper, .1f, 0.f);    
    ps->set_slow(slow.data());
  }

  std::unique_ptr<PhaseShift> ps;
  std::shared_ptr<complex4DReg> space4d;
  int n1, n2, n3, n4;
};

TEST_F(PS_Test, fwd) { 
  auto out = space4d->clone();
  ps->set_grid({32, 4, 4});
  ps->set_block({16, 16, 4});
  for (int i=0; i < 100; ++i)
    ps->forward(false, space4d, out);
}

TEST_F(PS_Test, adj) { 
  auto out = space4d->clone();
  for (int i=0; i < 100; ++i)
    ps->adjoint(false, out, space4d);
}

TEST_F(PS_Test, inv) { 
  auto out = space4d->clone();
  auto inv = space4d->clone();
  for (int i=0; i < 10; ++i) {
    ps->forward(false, out, space4d);
    ps->inverse(false, inv, space4d);
    inv->scaleAdd(out, 1, -1);
    double err = inv->norm(2) / out->norm(2);
    std::cout << err << " ";
    ASSERT_TRUE(err <= 1e-7); 
  }
}

TEST_F(PS_Test, dotTest) { 
  auto err = ps->dotTest(verbose);
  ASSERT_TRUE(err.first <= tolerance);
  ASSERT_TRUE(err.second <= tolerance);
}

class PSPI_Test : public testing::Test {
 protected:
  void SetUp() override {
    nx = 100;
    ny = 100;
    nz = 10;
    nw = 10;
    ns = 5;
    int nref = 3;

    auto slow4d = std::make_shared<complex4DReg>(nx, ny, nw, nz);
    slow4d->set(1.f);

    // create a vector of slowness values for each frequency
    auto domain = std::make_shared<hypercube>(nx, ny, nw, ns);
    space4d = std::make_shared<complex4DReg>(domain);
    space4d->set(1.f);

    Json::Value root;
    root["nref"] = nref;
    auto par = std::make_shared<jsonParamObj>(root);
    dim3 grid = {32, 4, 4};
    dim3 block = {16, 16, 4};
    pspi = std::make_unique<PSPI>(domain, slow4d, par, nullptr, nullptr, grid, block);
    pspi->set_depth(5);
  }

  std::unique_ptr<PSPI> pspi;
  std::shared_ptr<complex4DReg> space4d;
  int nx, ny, nz, nw, ns;
};

TEST_F(PSPI_Test, fwd) { 
  auto out = space4d->clone();
  for (int i=0; i < 3; ++i)
    ASSERT_NO_THROW(pspi->forward(false, space4d, out));
}

TEST_F(PSPI_Test, cu_fwd) { 
  auto out = space4d->clone();
  ASSERT_NO_THROW(pspi->cu_forward(false, pspi->model_vec, pspi->data_vec));
}

TEST_F(PSPI_Test, cu_inplace) { 
  auto out = space4d->clone();
  out->random();
  double before = std::real(out->dot(out));

  CHECK_CUDA_ERROR(cudaMemcpy(pspi->model_vec->mat, out->getVals(), pspi->getDomainSizeInBytes(), cudaMemcpyHostToDevice));
  ASSERT_NO_THROW(pspi->cu_forward(pspi->model_vec));
  CHECK_CUDA_ERROR(cudaMemcpy(out->getVals(), pspi->model_vec->mat, pspi->getDomainSizeInBytes(), cudaMemcpyDeviceToHost));
  
  double after = std::real(out->dot(out));
  ASSERT_TRUE(after != before);
  ASSERT_TRUE(after > 0.);
}

// TEST_F(PSPI_Test, inv) { 
//   auto out = space4d->clone();
//   auto inv = space4d->clone();
//   for (int i=0; i < 10; ++i) {
//     pspi->forward(false, out, space4d);
//     pspi->inverse(false, inv, space4d);
//     inv->scaleAdd(out, 1, -1);
//     double err = inv->norm(2) / out->norm(2);
//     std::cout << err << " ";
//     ASSERT_TRUE(err <= 1e-7); 
//   }
// }

TEST_F(PSPI_Test, dotTest) { 
  auto err = pspi->dotTest(verbose);
  ASSERT_TRUE(err.first <= tolerance);
  ASSERT_TRUE(err.second <= tolerance);
}


class Selector_Test : public testing::Test {
 protected:
  void SetUp() override {
    nx = 100;
    ny = 100;
    nz = 10;
    nw = 10;
    ns = 1;
    nref = 3;

    auto slow4d = std::make_shared<complex4DReg>(nx, ny, nw, nz);
    ref = std::make_shared<RefSampler>(slow4d, nref);
    slow4d->random();

    // create a vector of slowness values for each frequency
    auto domain = std::make_shared<hypercube>(nx, ny, nw, ns);
    space4d = std::make_shared<complex4DReg>(domain);
    space4d->set(1.f);

    select = std::make_unique<Selector>(domain);
  }

  std::unique_ptr<Selector> select;
  std::shared_ptr<complex4DReg> space4d;
  std::shared_ptr<RefSampler> ref;
  int nx, ny, nz, nw, ns, nref;
};

TEST_F(Selector_Test, dotTest) { 
  for (int iz = 0; iz < 3; ++iz) {
    select->set_labels(ref->get_ref_labels(iz));
    for (int iref = 0; iref < nref; ++iref) {
      select->set_value(iref);
      auto err = select->dotTest(verbose);
      ASSERT_TRUE(err.first <= tolerance);
      ASSERT_TRUE(err.second <= tolerance);
    }
  }
};

class Injection_Test : public testing::Test {
 protected:
  void SetUp() override {
    nx = 100;
    auto ax1 = axis(nx, 0.f, 0.01f);
    ny = 100;
    auto ax2 = axis(ny, 0.f, 0.01f);
    nw = 10;
    auto ax3 = axis(nw, 1.f, 1.f);
    ns = 5;
    auto ax4 = axis(ns, 0.f, 1.f);
    nz = 10;
    auto ax5 = axis(nz, 0.f, 0.01f);

    auto range = std::make_shared<hypercube>(ax1, ax2, ax3, ax4, ax5);
    wfld = std::make_shared<complex5DReg>(range);

    int ntrace = 20;
    traces = std::make_shared<complex2DReg>(nw, ntrace);
    auto domain = traces->getHyper();

    std::vector<float> cx(ntrace);
    std::vector<float> cy(ntrace);
    std::vector<float> cz(ntrace);
    std::vector<int> ids(ntrace);

    // Create a random number generator
    std::random_device rd;  // Obtain a random seed from the OS
    std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> distrib_x(ax1.o + ax1.d, (ax1.n-2)*ax1.d);
    std::uniform_real_distribution<> distrib_y(ax2.o + ax2.d, (ax2.n-2)*ax2.d);
    std::uniform_real_distribution<> distrib_z(ax5.o + ax5.d, (ax5.n-2)*ax5.d);
    std::uniform_real_distribution<> distrib_id(0, ns-1);

    // Generate the random coordinates
    for (int i = 0; i < ntrace; ++i) {
      cx[i] = distrib_x(gen);
      cy[i] = distrib_y(gen);
      cz[i] = distrib_z(gen);
      ids[i] = distrib_id(gen);
    }
    
    injection = std::make_unique<Injection>(domain, range, cx, cy, cz, ids);
  }

  std::unique_ptr<Injection> injection;
  int nx, ny, nz, nw, ns;
  std::shared_ptr<complex5DReg> wfld;
  std::shared_ptr<complex2DReg> traces;
};

TEST_F(Injection_Test, fwd) { 
  for (int i=0; i < 3; ++i)
    ASSERT_NO_THROW(injection->forward(false, traces, wfld));
}

TEST_F(Injection_Test, dotTest) { 
  auto err = injection->dotTest(verbose);
  ASSERT_TRUE(err.first <= tolerance);
  ASSERT_TRUE(err.second <= tolerance);
}

class UpDown_Test : public testing::Test {
 protected:
  void SetUp() override {
    nx = 100;
    auto ax1 = axis(nx, 0.f, 0.01f);
    ny = 100;
    auto ax2 = axis(ny, 0.f, 0.01f);
    nw = 10;
    auto ax3 = axis(nw, 1.f, 1.f);
    ns = 5;
    auto ax4 = axis(ns, 0.f, 1.f);
    nz = 10;
    auto ax5 = axis(nz, 0.f, 0.01f);

    auto domain = std::make_shared<hypercube>(ax1, ax2, ax3, ax4);
    wfld1 = std::make_shared<complex4DReg>(domain);
    wfld2 = std::make_shared<complex4DReg>(domain);

    auto slow4d = std::make_shared<complex4DReg>(nx, ny, nw, nz);
    slow4d->set(1.f);

    Json::Value root;
    root["nref"] = 3;
    auto par = std::make_shared<jsonParamObj>(root);

    down = std::make_unique<Downward>(domain, slow4d, par);
    // up = std::make_unique<Upward>(domain, slow4d, par);
  }

  std::unique_ptr<Downward> down;
  std::unique_ptr<Upward> up;
  int nx, ny, nz, nw, ns;
  std::shared_ptr<complex4DReg> wfld1, wfld2;
};

TEST_F(UpDown_Test, down_fwd) { 
  for (int i=0; i < 3; ++i)
    ASSERT_NO_THROW(down->forward(false, wfld1, wfld2));
}

TEST_F(UpDown_Test, down_fwd_wfld) { 
  auto wfld = down->get_wfld();
  ASSERT_EQ(std::real(wfld->dot(wfld)), 0.);
  wfld1->random();
  down->forward(false, wfld1, wfld2);

  ASSERT_TRUE(std::real(wfld->dot(wfld)) > 0.);
}

TEST_F(UpDown_Test, down_adj) { 
  for (int i=0; i < 3; ++i)
    ASSERT_NO_THROW(down->adjoint(false, wfld1, wfld2));
}

TEST_F(UpDown_Test, down_adj_wfld) { 
  auto wfld = down->get_wfld();
  ASSERT_EQ(std::real(wfld->dot(wfld)), 0.);
  wfld2->random();
  down->adjoint(false, wfld1, wfld2);
  ASSERT_EQ(std::real(wfld->dot(wfld)), 0.);
}
// TEST_F(UpDown_Test, up_fwd) { 
//   for (int i=0; i < 3; ++i)
//     ASSERT_NO_THROW(up->forward(false, wfld1, wfld2));
// }

// TEST_F(UpDown_Test, up_adj) { 
//   for (int i=0; i < 3; ++i)
//     ASSERT_NO_THROW(up->adjoint(false, wfld1, wfld2));
// }

TEST_F(UpDown_Test, down_dotTest) { 
  auto err = down->dotTest(verbose);
  ASSERT_TRUE(err.first <= tolerance);
  ASSERT_TRUE(err.second <= tolerance);
}
// TEST_F(UpDown_Test, up_dotTest) { 
//   up->dotTest(verbose);
// }

int main(int argc, char **argv) {
  // Parse command-line arguments
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--verbose") {
      verbose = true;
    }
    else if (std::string(argv[i]) == "--tolerance" && i + 1 < argc) {
      tolerance = std::stod(argv[i + 1]);
    }
  }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}