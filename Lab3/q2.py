import random
from collections import deque

import matplotlib.pyplot as plt


def generate_page_references(length: int, page_range: int) -> list[int]:
    return [random.randint(1, page_range) for _ in range(length)]


def fifo_page_replacement(pages: list[int], frame_count: int) -> int:
    frame = deque()
    page_faults = 0

    for page in pages:
        if page not in frame:
            if len(frame) == frame_count:
                frame.popleft()
            frame.append(page)
            page_faults += 1

    return page_faults


def lru_page_replacement(pages: list[int], frame_count: int) -> int:
    frame = []
    page_faults = 0

    for page in pages:
        if page in frame:
            frame.remove(page)
        else:
            page_faults += 1
            if len(frame) == frame_count:
                frame.pop(0)
        frame.append(page)

    return page_faults


def plot_faults(pages: list[int]) -> None:
    frame_range = range(1, 11)
    fifo_faults = [fifo_page_replacement(pages, f) for f in frame_range]
    lru_faults = [lru_page_replacement(pages, f) for f in frame_range]

    plt.plot(frame_range, fifo_faults, label='FIFO', marker='o')
    plt.plot(frame_range, lru_faults, label='LRU', marker='x')
    plt.xlabel('Number of Frames')
    plt.ylabel('Page Faults')
    plt.title('Page Faults vs Number of Frames')
    plt.legend()
    plt.grid(True)
    plt.savefig("q2_page_faults_plot.png")
    plt.show()


def main() -> None:
    page_references: list[int] = generate_page_references(1_000, 20)
    print("Page References:")
    print("------------------")
    print(" ".join(f"{i:2d}" for i in page_references))
    print("\nPage References (as list):")
    print("----------------------------")
    print(page_references)

    plot_faults(page_references)
    print("Page faults plotted and saved as 'q2_page_faults_plot.png'.")


if __name__ == "__main__":
    main()
