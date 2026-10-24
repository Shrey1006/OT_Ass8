# Prompt Log: I_quantum_algorithms - vqc_qsvm_vs_classical_benchmark
- **Timestamp**: 2026-10-24 17:01:30
- **Track**: I_quantum_algorithms
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
```text
I2. Train a variational classifier and a quantum kernel SVM versus an equivalent classical model on the same dataset. Report accuracy, training time, parameter count and circuit evaluations.
```

## AI Response (Summary / Relevant Excerpt)
```text
1. Dataset: Scikit-learn make_moons (N=100, noise=0.15) with 70/30 train/test split.
2. Trained 4 models on identical data:
   - Quantum Kernel SVM (QSVM with ZZFeatureMap)
   - Variational Quantum Classifier (VQC with parameter-shift rule)
   - Classical RBF-Kernel SVM
   - Classical Small MLP (9 parameters)
3. Reported complete comparative table: Test Accuracy (%), Training Time (s), Parameter Count, Circuit / Model Evaluations.
4. Comprehensive writeup committed to report/I_quantum_algorithms.md.
```

## Revised Prompt / User Iteration
```text
Initial implementation of Step I2.
```

## Commit Link
- Commit Hash / Tag: `[AI:Antigravity] Train VQC and Quantum Kernel SVM vs Classical RBF and MLP on make_moons dataset`

## Verification Note
1. Correctness Check: Verified all models evaluate on identical train and test splits with deterministic random seeds.
2. Baseline & Fair Comparison: Compared QSVM and VQC against both a classical kernel method (RBF-SVM) and a classical neural net (MLP).
3. Claims Verified: Accurately reported VQC parameter compactness (6 params) alongside its high evaluation overhead (11,550 circuit evaluations).
