import os
import pandas as pd
import matplotlib.pyplot as plt


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

RESULTS_FILE = os.path.join(PROJECT_ROOT, "results", "results.csv")
PLOTS_DIR = os.path.join(PROJECT_ROOT, "plots")


def load_results():
    if not os.path.exists(RESULTS_FILE):
        raise FileNotFoundError(f"Results file not found: {RESULTS_FILE}")

    df = pd.read_csv(RESULTS_FILE)

    required_columns = {
        "distance_m",
        "throughput_mbps",
        "delay_ms",
        "packet_loss_percent",
        "tx_packets",
        "rx_packets",
    }

    missing_columns = required_columns - set(df.columns)
    if missing_columns:
        raise ValueError(f"Missing columns in CSV file: {missing_columns}")

    df = df.sort_values("distance_m")
    return df


def ensure_plots_dir():
    os.makedirs(PLOTS_DIR, exist_ok=True)


def plot_throughput(df):
    plt.figure(figsize=(8, 5))
    plt.plot(df["distance_m"], df["throughput_mbps"], marker="o")
    plt.xlabel("Distance between AP and STA (m)")
    plt.ylabel("Throughput (Mbps)")
    plt.title("Throughput vs Distance")
    plt.grid(True)
    plt.tight_layout()

    output_path = os.path.join(PLOTS_DIR, "throughput_vs_distance.png")
    plt.savefig(output_path, dpi=300)
    plt.close()

    print(f"Saved: {output_path}")


def plot_delay(df):
    plt.figure(figsize=(8, 5))
    plt.plot(df["distance_m"], df["delay_ms"], marker="o")
    plt.xlabel("Distance between AP and STA (m)")
    plt.ylabel("Average Delay (ms)")
    plt.title("Average Delay vs Distance")
    plt.grid(True)
    plt.tight_layout()

    output_path = os.path.join(PLOTS_DIR, "delay_vs_distance.png")
    plt.savefig(output_path, dpi=300)
    plt.close()

    print(f"Saved: {output_path}")


def plot_packet_loss(df):
    plt.figure(figsize=(8, 5))
    plt.plot(df["distance_m"], df["packet_loss_percent"], marker="o")
    plt.xlabel("Distance between AP and STA (m)")
    plt.ylabel("Packet Loss (%)")
    plt.title("Packet Loss vs Distance")
    plt.grid(True)
    plt.tight_layout()

    output_path = os.path.join(PLOTS_DIR, "packet_loss_vs_distance.png")
    plt.savefig(output_path, dpi=300)
    plt.close()

    print(f"Saved: {output_path}")


def print_summary(df):
    print("\nSimulation results:")
    print(df.to_string(index=False))

    best = df.loc[df["throughput_mbps"].idxmax()]
    worst = df.loc[df["throughput_mbps"].idxmin()]

    print("\nSummary:")
    print(
        f"Highest throughput: {best['throughput_mbps']:.4f} Mbps "
        f"at {best['distance_m']} m"
    )
    print(
        f"Lowest throughput: {worst['throughput_mbps']:.4f} Mbps "
        f"at {worst['distance_m']} m"
    )


def main():
    ensure_plots_dir()
    df = load_results()

    print_summary(df)

    plot_throughput(df)
    plot_delay(df)
    plot_packet_loss(df)


if __name__ == "__main__":
    main()