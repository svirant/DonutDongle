# Donut Dongle
Arduino Nano based hub that connects your current console switching setup with the RetroTink 4K and/or RetroTink 5x for Auto Profile switching

<img width="600" alt="donutdongle_v04" src="https://github.com/user-attachments/assets/57490759-f556-44dc-b4ce-beb28def27d4" />

## Donut Dongle Bill of Materials (BOM)
### [- Digikey shared list](https://www.digikey.com/en/mylists/list/53XEHTQZJK)
### [- AliExpress wish list](https://www.aliexpress.com/p/wish-manage/share.html?smbPageCode=wishlist-amp&spreadId=09A40E28BA67E42DE9AF29A70E7238263DE305046632ABE9A0D5E1C5C4589AD9)
* **1x** PJ-307 3.5mm Stereo Jack: [AliExpress](https://www.aliexpress.us/item/2251832712518602.html)
  - or **1x** PJ-324M 3.5mm Audio Jack Socket: [AliExpress](https://www.aliexpress.us/item/2251832685563184.html)

* **4x** PJ-320 3.5MM Headphone Jack Audio Video Female: [AliExpress](https://www.aliexpress.us/item/3256805995568762.html)
  
* **2x** 2x5 Pin Double Row 2.54mm Pitch Straight Box Header: [AliExpress](https://www.aliexpress.us/item/3256805177947724.html) (Color: STRAIGHT TYPE, Pins: 10PCS DC3-10Pin)
  
* **2x** 2x4 Pin Double Row 2.54mm Pitch Straight Box Header: [AliExpress](https://www.aliexpress.us/item/3256805177947724.html) (Color: STRAIGHT TYPE, Pins: 10PCS DC3-8Pin)
  
* **1x** 3.5mm CHF03 1.5 Meters IR Infrared Remote Emission Cable: [AliExpress](https://www.aliexpress.us/item/3256805962345169.html)
  
* **1x** 3.5mm Infrared Remote Control Receiver Extension Cable: [AliExpress](https://www.aliexpress.us/item/2251832741040177.html)
  
* **1x   (U1)** MAX3232 SOP-16 RS-232 Interface IC: [AliExpress](https://www.aliexpress.us/item/3256807314260762.html)
  
* **1x   (Q1)** 2N3904 NPN Transistor: [AliExpress](https://www.aliexpress.us/item/3256806623522970.html)
  
* **2x** 2.54mm Pitch Single Row Female 15P Header Strip: [AliExpress](https://www.aliexpress.us/item/3256801232229618.html)

* **4x  (C1-C4)** 0.1 uf / 100nf 0805 Capacitor: [Digikey](https://www.digikey.com/en/products/detail/yageo/CC0805KRX7R9BB104/302874?s=N4IgTCBcDaIMwEYEFokBYAMrkDkAiIAugL5A)
  
* **1x  (R1)** 30 Ohm 0805 Resistor: [Digikey](https://www.digikey.com/en/products/detail/panasonic-electronic-components/ERJ-P06J300V/525250)
  
* **1x  (R2)** 1K Ohm 0805 Resistor: [Digikey](https://www.digikey.com/en/products/detail/panasonic-electronic-components/ERJ-P06J102V/525197)

* **6x  (R3-R8)** 10K Ohm 0805 Resistor: [Digikey](https://www.digikey.com/en/products/detail/panasonic-electronic-components/ERJ-6ENF1002V/111474)

* **1x** Any 3.5mm / aux / stereo / trs / cable: [AliExpress](https://www.aliexpress.us/item/2255799962255486.html)

* **1x** usb-c cable for Arduino power & initial programming: [AliExpress](https://www.aliexpress.us/item/3256806983355947.html)

* **1x [for TESmart HDMI]** Any 3.5mm / aux / stereo / trs / Cable: [AliExpress](https://www.aliexpress.us/item/2255799962255486.html)

* **1x [for GscartSW/GCompSW]** 2x4 Pin Double Row 2.54 Pitch Angled Box Header: [AliExpress](https://www.aliexpress.us/item/3256805177947724.html?) (Color: RIGHT ANGLE TYPE, Pins: 10PCS DC3 8Pin)

* **1 or 2x [for GscartSW/GCompSW]** 2x4 Pin Female Header Ribbon Cable: [AliExpress](https://www.aliexpress.us/item/3256804576275377.html?) (Pins: 2x4Pin, Color: Any length)
  
* **1x [for Extron]** 2 Port DB9 to 2x5 Pin Female Header Ribbon Cable: [AliExpress](https://www.aliexpress.us/item/3256807472891897.html)

  - or **2x** DB9 Male to 3.5mm Male Serial RS232 Cable 6feet: [Amazon](https://www.amazon.com/LIANSHU-DC3-5mm-Serial-RS232-Cable/dp/B07G2ZL3SL/)
  
  - "MUST" be wired as so: [DB9 Male Pin 5 -> Sleeve, DB9 Male Pin 2 -> Tip, DB9 Male Pin 3 -> Ring](https://github.com/user-attachments/assets/4660ba77-eace-4b76-b169-7ea5f80491f9)
 


### - at least 1 of the following [VGA adapters](https://github.com/svirant/DonutDongle/tree/main/Adapters) is required:

* **1x** YC2VGA w/ Serial Tap:
  
  - Yellow RCA Composite jack: [AliExpress](https://www.aliexpress.us/item/3256805949533421.html) (Color: Yellow)
    
  - S-Video jack: [AliExpress](https://www.aliexpress.us/item/3256804041313042.html) (Color: A 4Pin)
    
  - PJ-320 3.5MM Headphone Jack Audio Video Female: [AliExpress](https://www.aliexpress.us/item/3256805995568762.html) (Color: 10PCS PJ-320)
    
  - VGA Male Connector [L717HDE15PD4CH4R-ND]: [Digikey](https://www.digikey.com/en/products/detail/amphenol-cs-commercial-products/L717HDE15PD4CH4R/4886543)
 
* **1x** VGA Passthrough w/ Serial Tap:
  
  - VGA Male Connector [L717HDE15PD4CH4R-ND]: [Digikey](https://www.digikey.com/en/products/detail/amphenol-cs-commercial-products/L717HDE15PD4CH4R/4886543)
    
  - VGA Female Connector: [AliExpress](https://www.aliexpress.us/item/3256806106796466.html) (Color: HDR15P Female 3.08)
    
  - PJ-320 3.5MM Headphone Jack Audio Video Female: [AliExpress](https://www.aliexpress.us/item/3256805995568762.html) (Color: 10PCS PJ-320)

  
