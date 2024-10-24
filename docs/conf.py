# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'SuperCollider'
copyright = '2024, SuperCollider community'
author = 'SuperCollider community'
# release = '3.14.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.autosummary',
    'breathe',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'venv']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "furo"
html_static_path = ['_static']
html_logo = "../icons/sc_logo.svg"

breathe_projects = {
    "common": "common/xml",
    "include": "include/xml",
    "lang": "lang/xml",
    "QtCollider": "QtCollider/xml",
    "sc-ide": "sc-ide/xml",
    "SCDoc": "SCDoc/xml",
    "scsynth": "scsynth/xml",
    "supernova": "supernova/xml",
}

breathe_default_members = ('members', 'undoc-members')
