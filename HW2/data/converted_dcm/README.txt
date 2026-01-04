These are extracted DICOM files (renamed with .dcm where missing).
You can run the encoder on one file, e.g.:
  python encode.py --input /mnt/data/extracted_2_skull_ct/dicom/I0.dcm --output /mnt/data/results/q30.bin --quality 30
Or use the provided read_and_save.py to convert a DICOM to 16-bit TIFF for inspection.
