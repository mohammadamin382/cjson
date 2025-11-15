from setuptools import setup, Extension

module = Extension(
    'cjson',
    sources=[
        './src/python_bindings.c',
        './src/json_parser.c',
        './src/json_stringify.c',
        './src/json_converters.c',
        './src/json_utils.c',
        './src/json_file_io.c',
        './src/json_to_parsers.c',
        './src/json_sqlite.c',
        './src/json_advanced.c'
    ],
    include_dirs=['./src'],
    libraries=['sqlite3'],
    extra_compile_args=['-std=c11', '-O3']
)

setup(
    name='cjson',
    version='1.0.0',
    description='JSON library',
    ext_modules=[module],
    author='mohammad amin',
    author_email='moahmmad.hosseinii@gmail.com',
    url='https://github.com/mohammadamin382/cjson',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU General Public License v3.0 (GPL-3.0)',
        'Programming Language :: C',
        'Programming Language :: Python :: 3',
    ],
)
