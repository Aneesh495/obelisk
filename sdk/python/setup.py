from setuptools import setup, find_packages

setup(
    name="obelisk-db",
    version="1.0.0",
    description="Python client driver for the Obelisk transactional database engine",
    author="Aneesh",
    packages=find_packages(),
    python_requires=">=3.8",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Topic :: Database :: Database Engines/Servers",
    ],
)
