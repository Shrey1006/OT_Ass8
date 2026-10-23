#!/usr/bin/env python3
"""
plot_results.py - Generates publication-ready figures for Track A continuous optimization.
Visualizes:
 1. Convergence trajectories on Convex vs. Strongly Convex vs. Rosenbrock vs. Saddle Point.
 2. Learning rate sensitivity analysis across the parameter grid.
 3. Stochastic noise resilience (Mean +/- Std over 10 independent seeds).

Course: Optimization Techniques | Assignment 8 (Track A)
"""

import os
import sys
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def load_trajectories(csv_path):
    trajectories = {}
    with open(csv_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            obj = row['Objective']
            opt = row['Optimizer']
            step = int(row['Iteration'])
            loss = float(row['Loss'])
            if obj not in trajectories:
                trajectories[obj] = {}
            if opt not in trajectories[obj]:
                trajectories[obj][opt] = []
            trajectories[obj][opt].append((step, loss))
    return trajectories

def load_lr_sensitivity(csv_path):
    data = {}
    with open(csv_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            obj = row['Objective']
            opt = row['Optimizer']
            lr = float(row['Learning_Rate'])
            loss = float(row['Final_Loss'])
            if obj not in data:
                data[obj] = {}
            if opt not in data[obj]:
                data[obj][opt] = []
            data[obj][opt].append((lr, loss))
    return data

def load_summary(csv_path):
    records = []
    with open(csv_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            records.append({
                'Objective': row['Objective'],
                'Landscape': row['Landscape_Type'],
                'Optimizer': row['Optimizer'],
                'Best_LR': float(row['Best_LR']),
                'Clean_Loss': float(row['Clean_Loss']),
                'Noisy_Mean': float(row['Noisy_Loss_Mean']),
                'Noisy_Std': float(row['Noisy_Loss_Std'])
            })
    return records

def plot_convergence_comparisons(trajectories, out_path):
    target_objs = [
        ("ConvexQuadratic", "Convex Quadratic (Well-conditioned)"),
        ("StronglyConvexQuadratic (kappa=100)", "Strongly Convex Quadratic (kappa=100)"),
        ("Rosenbrock", "Rosenbrock (Non-Convex Banana Valley)"),
        ("SaddlePoint (x^2 - y^2)", "Saddle Point (f = x^2 - y^2, Negative Curvature)")
    ]

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()

    colors = {
        'GradientDescent': '#1f77b4',
        'SGD': '#aec7e8',
        'MiniBatchSGD': '#ff7f0e',
        'Momentum': '#2ca02c',
        'NAG': '#98df8a',
        'AdaGrad': '#d62728',
        'RMSProp': '#9467bd',
        'Adam': '#8c564b'
    }

    for idx, (obj_key, title) in enumerate(target_objs):
        ax = axes[idx]
        if obj_key in trajectories:
            for opt_name, points in trajectories[obj_key].items():
                points.sort(key=lambda x: x[0])
                steps = [p[0] for p in points][:150]
                losses = [p[1] for p in points][:150]

                # Format saddle point or negative values
                if "Saddle" in obj_key:
                    ax.plot(steps, losses, label=opt_name, color=colors.get(opt_name, 'black'), lw=1.8)
                    ax.set_ylabel("f(x, y) [Descent towards -inf]")
                else:
                    # Clip small positive values for log scale
                    clipped_losses = [max(l, 1e-12) for l in losses]
                    ax.semilogy(steps, clipped_losses, label=opt_name, color=colors.get(opt_name, 'black'), lw=1.8)
                    ax.set_ylabel("Loss f(x, y) (log scale)")

        ax.set_title(title, fontsize=12, fontweight='bold')
        ax.set_xlabel("Iteration Step")
        ax.grid(True, linestyle='--', alpha=0.6)
        if idx == 0:
            ax.legend(fontsize=9, loc='upper right')

    plt.suptitle("Continuous Optimization: Convex vs. Ill-Conditioned vs. Non-Convex vs. Saddle", fontsize=15, fontweight='bold')
    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"Saved convergence plot: {out_path}")

def plot_lr_sensitivity(lr_data, out_path):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    
    # 1. Strongly Convex Quadratic
    ax1 = axes[0]
    scq_key = "StronglyConvexQuadratic (kappa=100)"
    if scq_key in lr_data:
        for opt_name, pts in lr_data[scq_key].items():
            pts.sort(key=lambda x: x[0])
            lrs = [p[0] for p in pts]
            losses = [min(max(p[1], 1e-8), 1e4) for p in pts]
            ax1.loglog(lrs, losses, marker='o', label=opt_name)
    ax1.set_title("Step-Size Sensitivity: Strongly Convex (kappa=100)", fontweight='bold')
    ax1.set_xlabel("Learning Rate (eta)")
    ax1.set_ylabel("Final Loss (log scale)")
    ax1.grid(True, which='both', linestyle='--', alpha=0.5)
    ax1.legend(fontsize=9)

    # 2. Rosenbrock
    ax2 = axes[1]
    rosen_key = "Rosenbrock"
    if rosen_key in lr_data:
        for opt_name, pts in lr_data[rosen_key].items():
            pts.sort(key=lambda x: x[0])
            lrs = [p[0] for p in pts]
            losses = [min(max(p[1], 1e-4), 1e4) for p in pts]
            ax2.loglog(lrs, losses, marker='s', label=opt_name)
    ax2.set_title("Step-Size Sensitivity: Rosenbrock Valley", fontweight='bold')
    ax2.set_xlabel("Learning Rate (eta)")
    ax2.set_ylabel("Final Loss (log scale)")
    ax2.grid(True, which='both', linestyle='--', alpha=0.5)

    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"Saved LR sensitivity plot: {out_path}")

def plot_noisy_bar(records, out_path):
    # Filter for Strongly Convex Quadratic and Rosenbrock
    selected_objs = ["ConvexQuadratic", "Rosenbrock"]
    filtered = [r for r in records if r['Objective'] in selected_objs]

    if not filtered:
        return

    optimizers = sorted(list(set(r['Optimizer'] for r in filtered)))
    x = np.arange(len(optimizers))
    width = 0.35

    fig, ax = plt.subplots(figsize=(12, 6))

    for idx, obj in enumerate(selected_objs):
        means = []
        stds = []
        for opt in optimizers:
            entry = next((r for r in filtered if r['Objective'] == obj and r['Optimizer'] == opt), None)
            if entry:
                means.append(max(entry['Noisy_Mean'], 1e-6))
                stds.append(entry['Noisy_Std'])
            else:
                means.append(0.0)
                stds.append(0.0)

        offset = (idx - 0.5) * width
        ax.bar(x + offset, means, width, yerr=stds, capsize=4, label=obj, alpha=0.85)

    ax.set_ylabel("Final Objective Value (Mean +/- Std, 10 Seeds)")
    ax.set_title("Stochastic Robustness under Noisy Gradients (10 Runs)", fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(optimizers, rotation=25)
    ax.legend()
    ax.grid(True, axis='y', linestyle='--', alpha=0.6)

    plt.tight_layout()
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"Saved noisy benchmark bar plot: {out_path}")

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    track_dir = os.path.dirname(base_dir)
    results_dir = os.path.join(track_dir, "results")

    traj_path = os.path.join(results_dir, "convergence_trajectories.csv")
    lr_path = os.path.join(results_dir, "lr_sensitivity.csv")
    summary_path = os.path.join(results_dir, "benchmark_summary.csv")

    if not os.path.exists(traj_path) or not os.path.exists(lr_path) or not os.path.exists(summary_path):
        print(f"Error: CSV files not found in {results_dir}. Run the C++ benchmark executable first.")
        sys.exit(1)

    trajectories = load_trajectories(traj_path)
    lr_data = load_lr_sensitivity(lr_path)
    records = load_summary(summary_path)

    out_conv = os.path.join(results_dir, "convergence_convex_vs_nonconvex_vs_saddle.png")
    out_lr = os.path.join(results_dir, "learning_rate_sensitivity.png")
    out_noisy = os.path.join(results_dir, "noisy_gradient_comparison.png")

    plot_convergence_comparisons(trajectories, out_conv)
    plot_lr_sensitivity(lr_data, out_lr)
    plot_noisy_bar(records, out_noisy)

    print("All visualization plots generated successfully in results/ directory.")

if __name__ == '__main__':
    main()
