#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

constexpr int N = 100;         // number of neurons
constexpr double p_conn = 0.1; // connection probability
constexpr double dt = 0.1;     // time step in ms
constexpr double T = 1000.0;   // total simulation time in ms
constexpr int steps = static_cast<int>(T / dt);

constexpr double V_rest = -70.0; // mV
constexpr double V_th = -55.0;   // mV
constexpr double E_rev = 0.0;    // mV
constexpr double R_m = 0.1;      // GOhm
constexpr double tau_m = 10.0;   // ms
constexpr double tau_s = 5.0;    // ms
constexpr double g_s = 0.01;     // uS == 10 nS
constexpr double I_s = 0.0001;   // uA == 100 pA
constexpr double p_s = 1e-4;     // spontaneous firing prob per step
constexpr double tau_ref = 2.0;  // refractory period in ms
constexpr int ref_steps = static_cast<int>(tau_ref / dt);

bool use_conductance_based = true; // switch model type

std::mt19937 rng(std::random_device{}());
std::uniform_real_distribution<> uni(0.0, 1.0);

// Spike raster data
std::vector<std::pair<int, double>> spike_times;

class Neuron {
public:
  double V = V_rest;
  double g = 0.0;     // conductance
  double I_syn = 0.0; // synaptic current
  int refract_timer = 0;

  std::vector<int> post_synaptic;
  std::vector<double> spike_history;

  void update(int t, double current_input, char &spiked) {
    if (refract_timer > 0) {
      refract_timer--;
      V = V_rest;
      spiked = 0;
      return;
    }

    // Spontaneous firing
    if (uni(rng) < p_s) {
      fire(t);
      spiked = 1;
      return;
    }

    // Update conductance-based or current-based synaptic current
    if (use_conductance_based) {
      I_syn += -(g / tau_s) * dt;
      I_syn = std::max(0.0, I_syn);
      double dV = dt / tau_m * (-(V - V_rest) + R_m * g * (E_rev - V));
      V += dV;
    } else {
      I_syn += -(I_syn / tau_s) * dt;
      double dV = dt / tau_m * (-(V - V_rest) + R_m * I_syn);
      V += dV;
    }

    spiked = 0;
    if (V >= V_th) {
      fire(t);
      spiked = 1;
    }
  }

  void receive_spike() {
    if (use_conductance_based)
      g += g_s;
    else
      I_syn += I_s;
  }

private:
  void fire(int t) {
    V = V_rest;
    refract_timer = ref_steps;
    spike_history.push_back(t * dt);
  }
};

std::vector<Neuron> neurons;

void connect_neurons() {
  std::bernoulli_distribution conn_dist(p_conn);
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      if (i != j && conn_dist(rng)) {
        neurons[i].post_synaptic.push_back(j);
      }
    }
  }
}

void simulate() {
  for (int t = 0; t < steps; ++t) {
    std::vector<char> fired(N, 0);

    // Update all neurons
    for (int i = 0; i < N; ++i) {
      neurons[i].update(t, 0.0, fired[i]);
      if (fired[i]) {
        spike_times.emplace_back(i, t * dt);
      }
    }

    // Propagate spikes
    for (int i = 0; i < N; ++i) {
      if (fired[i]) {
        for (int target : neurons[i].post_synaptic) {
          neurons[target].receive_spike();
        }
      }
    }

    // Periodic sync point
    if (t % 100 == 0) {
      std::cout << "Step " << t << "/" << steps << "\r" << std::flush;
    }
  }
  std::cout << "\nSimulation done.\n";
}

void save_spikes(const std::string &filename) {
  std::ofstream out(filename);
  for (auto &[id, time] : spike_times) {
    out << time << "," << id << "\n";
  }
  std::cout << "Spikes saved to " << filename << "\n";
}

int main() {
  neurons.resize(N);
  connect_neurons();

  auto start = std::chrono::high_resolution_clock::now();
  simulate();
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Simulation time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms\n";

  save_spikes("spikes.csv");

  return 0;
}
