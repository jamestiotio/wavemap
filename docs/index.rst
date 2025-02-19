Wavemap documentation
#####################

.. image:: https://github.com/ethz-asl/wavemap/assets/6238939/0df66963-3871-4fae-8567-523518c43494
  :alt: 3D reconstruction of Newer College's Cloister

Hierarchical, multi-resolution volumetric mapping
*************************************************

Wavemap achieves state-of-the-art memory and computational efficiency by combining Haar wavelet compression and a coarse-to-fine measurement integration scheme. Advanced measurement models allow it to attain exceptionally high recall rates on challenging obstacles like thin objects.

The framework is very flexible and supports several data structures, measurement integration methods, and sensor models out of the box. The ROS interface can, for example, easily be configured to fuse multiple sensor inputs, such as a LiDAR configured with a range of 20m and several depth cameras up to a resolution of 1cm, into a single map.

.. toctree::
   :maxdepth: 1
   :caption: Table of contents:

   pages/installation
   pages/demos
   pages/configuration
   pages/usage_examples
   pages/contributing
   api/library_root
   pages/faq


Paper
*****

A technical introduction to the theory behind the wavemap is provided in our open-access `RSS paper <https://www.roboticsproceedings.org/rss19/p065.pdf>`_.

Abstract:
    Volumetric maps are widely used in robotics due to their desirable properties in applications such as path planning, exploration, and manipulation. Constant advances in mapping technologies are needed to keep up with the improvements in sensor technology, generating increasingly vast amounts of precise measurements. Handling this data in a computationally and memory-efficient manner is paramount to representing the environment at the desired scales and resolutions. In this work, we express the desirable properties of a volumetric mapping framework through the lens of multi-resolution analysis. This shows that wavelets are a natural foundation for hierarchical and multi-resolution volumetric mapping. Based on this insight we design an efficient mapping system that uses wavelet decomposition. The efficiency of the system enables the use of uncertainty-aware sensor models, improving the quality of the maps. Experiments on both synthetic and real-world data provide mapping accuracy and runtime performance comparisons with state-of-the-art methods on both RGB-D and 3D LiDAR data. The framework is open-sourced to allow the robotics community at large to explore this approach.

Please cite it when using wavemap for research:

    Reijgwart, V., Cadena, C., Siegwart, R., & Ott, L. (2023). Efficient volumetric mapping of multi-scale environments using wavelet-based compression. Proceedings of Robotics: Science and System XIX.

BibTeX::

    @INPROCEEDINGS{reijgwart2023wavemap,
        author = {Reijgwart, Victor and Cadena, Cesar and Siegwart, Roland and Ott, Lionel},
        journal = {Robotics: Science and Systems. Online Proceedings},
        title = {Efficient volumetric mapping of multi-scale environments using wavelet-based compression},
        year = {2023-07},
    }


Note that the code has significantly improved since the paper was written. Wavemap is now up to 10x faster, thanks to new multi-threaded measurement integrators, and uses up to 50% less RAM, by virtue of new memory efficient data structures inspired by `OpenVDB <https://github.com/AcademySoftwareFoundation/openvdb>`_.
