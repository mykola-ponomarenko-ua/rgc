from setuptools import Extension, setup


setup(
    ext_modules=[
        Extension(
            name="rgc._rgc",
            sources=[
                "src/rgc/_rgcmodule.c",
                "src/rgc/rgc.c",
            ],
        )
    ]
)
