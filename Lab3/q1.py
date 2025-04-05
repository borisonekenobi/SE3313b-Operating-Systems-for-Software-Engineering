import random


def generate_page_references(length: int, page_range: int) -> list[int]:
    return [random.randint(1, page_range) for _ in range(length)]


def main() -> None:
    page_references: list[int] = generate_page_references(1_000, 20)
    print("Page References:")
    print("------------------")
    print(" ".join(f"{i:2d}" for i in page_references))
    print("\nPage References (as list):")
    print("----------------------------")
    print(page_references)


if __name__ == "__main__":
    main()
