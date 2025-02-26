# Intsy
32/64/128 channel bioamplifier system, now with accelerometer and on-board SD data logging capability for ambulatory applications! Photos of the original 32/64 channel device are below Also, check out the quick overview of the device internals and mechanical enclosure of the 128 channel ambulatory system in <a href =  "https://www.youtube.com/watch?v=RvYDtEdVOBU&feature=youtu.be">this video</a> (full credit to Riwaj Shrestha, Suyash Mathema, and Jeremy Wharton!).

Intsy is a validated, low-cost, open-source, wireless-enabled bioamplifier system.  The system can be configured for 32, 64, or 128 channel operation.  It has been validated for acquiring high-quality gastrointesinal slow wave and surface EMG electrical signals.  For more details, see our <a href =  "http://iopscience.iop.org/article/10.1088/1361-6579/aaad51">published peer-reviewed article in Physiological Measurement</a>. We have also regularly used Intsy to make <a href = "https://ieeexplore.ieee.org/abstract/document/8839813"> body surface recordings of colonic motilty patterns</a> using  conventiona wet (gel) as well as dry electrodes (see <a  href = "https://link.springer.com/article/10.1007/s10439-023-03137-w"> this journal article  </a>) 

Data can be streamed either in wired configuration over USB or wirelessly via bluetooth.  A LabView GUI allows for real-time visualization and saving to hard disk for off-line analysis.  Data rates of 32 chan x 110 Hz can be stably transmitted over bluetooth.  The USB wired configuration can achieve up to 2 kHz x 128 chan.  Matlab functions for converting binary Intsy data files into matlab format are provided.

The name Intsy comes from the two core components: Intan RHD2132 bioamplifier chip + Teensy 3.6 microcontroller.

Please see "Intsy_Setup_Guide.pdf" for detailed instructions on how to set up the Intsy system. Assembly time is typically about 4 hrs.  Total cost is about $1500 for the 32 channel system,  $2400 for the 64 channel system, and $6000 for the 128 channel system (the dominant cost being the Intan RHD2132 amplifier boards).  That means  you can build your very own high-quality, low-cost, portable bioamplifier system in just a single afternoon!

All feedback and suggestions are warmly welcomed. 

<p align="left">
  <img src="SystemOverview_Intsy128_Panel_abcd_v2_crop.png" width="350"/>
</p>
Left: Intsy 128 channel electronics and mechanical enclosure (circa May 2020)


<p align="right">
  <img src="SystemOverview_v1_300dpi.png" width="350"/>
</p>
Above: Intsy hardware architecture and device (32/64 channel original module circa Dec 2017)




<p align="center">
  <img src="SW_Intsy_screenshot.png" width="700"/>
</p>
Above: Screenshot of Intsy Labview front end to visualize and stream signals in real time.


<p></p>
<p><font size="16"><b>Disclaimer</b></font></p>
The Intsy electronics module is not a medical device nor is it intended for medical diagnosis and provided to you "as is," and we make no express or implied warranties whatsoever with respect to its functionality, operability, or use, including, without limitation, any implied warranties, fitness for a particular purpose, or infringement. We expressly disclaim any liability whatsoever for any direct, indirect, consequential, incidental or special damages, including, without limitation, lost revenues, lost profits, losses resulting from business interruption or loss of data, regardless of the form of action or legal theory under which the liability may be asserted, even if advised of the possibility of such damages. This bioamplifier module does not fall within the scope of the European Union directives regarding electromagnetic compatibility, restricted substances (RoHS), recycling (WEEE), FCC, CE or UL, and therefore may not meet the technical requirements of these directives or other related directives
