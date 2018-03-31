NAME
====

s1kd-newupf - Create a new data update file

SYNOPSIS
========

    s1kd-newupf [options] <SOURCE> <TARGET>

DESCRIPTION
===========

The *s1kd-newupf* tool creates a new S1000D data update file for two specified issues of a CIR data module. Changes to items between the source and target issues of the CIR are recorded in the resulting UPF, along with update instructions.

OPTIONS
=======

-@ &lt;filename&gt;  
Save the new UPF as &lt;filename&gt; instead of an automatically named file in the current directory.

-$ &lt;issue&gt;  
Specify which issue of S1000D to use. Currently supported issues are:

-   4.2 (default)

-   4.1

-% &lt;dir&gt;  
Use XML template in the specified directory instead of the built-in template. The template must be named `update.xml` in the directory &lt;dir&gt;, and must conform to the default S1000D issue of this tool (4.2).

-d &lt;defaults&gt;  
Specify the 'defaults' file name.

-f  
Overwrite existing file.

&lt;SOURCE&gt;  
The source (original) issue of the CIR data module.

&lt;TARGET&gt;  
The target (updated) issue of the CIR data module.