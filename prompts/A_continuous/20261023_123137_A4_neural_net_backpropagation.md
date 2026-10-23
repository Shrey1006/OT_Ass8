# Prompt Log: A_continuous - A4_neural_net_backpropagation
- **Timestamp**: 2026-10-23 12:31:37
- **Track**: A_continuous
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
A4. Implement a 2-layer neural network with hand-written backpropagation (no libraries), train it on a small dataset using each optimizer, and verify gradients with finite differences. Explain in comments why backprop is different from the optimizer.
```

## AI Response (Summary / Relevant Excerpt)
```text
Implemented opt::TwoLayerMLP in A_continuous/src/neural_net.hpp with hand-written backpropagation:
- Architecture: 2 inputs -> 4 tanh hidden neurons -> 1 linear output neuron (17 total parameters).
- Forward pass, Mean Squared Error loss, and reverse-mode analytical backpropagation computing exact dL/dW1, dL/db1, dL/dW2, dL/db2 via the chain rule.
- Added comprehensive comments explaining the fundamental theoretical distinction: Backpropagation is a pure gradient evaluation engine on computational DAGs (computing \nabla_\theta L), whereas the Optimizer is the search algorithm determining the displacement update \Delta \theta (handling learning rates, momentum, curvature corrections, and stopping criteria).
- Verified analytical backprop gradients against central finite differences (< 1e-5 on all 17 parameters).
- Trained the MLP across all 8 continuous optimizers (GD, SGD, MiniBatchSGD, Momentum, NAG, AdaGrad, RMSProp, Adam) from identical initial parameters, verifying consistent loss reduction (98.6% to 100.0%).
```

## Revised Prompt / User Iteration
```text
N/A (First-pass implementation)
```

## Commit Link
- Commit Message: `[AI:Antigravity] Implement 2-layer neural network with hand-written backpropagation, finite difference gradient verification, and 8-optimizer training`

## Verification Note
1. Correctness Check: Verified analytical backprop gradients against central finite differences with max coordinate error $< 10^{-5}$ across all 17 network parameters.
2. Baseline & Fair Comparison: Network trained across all 8 optimizers from identical Xavier weight initializations and deterministic seed (42), achieving $> 98\%$ loss reductions.
3. Claims Verified: Conceptual separation between backpropagation (gradient oracle) and optimizer (parameter displacement) thoroughly documented in code; verified zero compiler warnings under `-Wall -Wextra -Wpedantic`.
