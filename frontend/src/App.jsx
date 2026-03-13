import { useState } from 'react';
import axios from 'axios';
import { motion, AnimatePresence } from 'framer-motion';
import { Lock, Unlock, Upload, FileAudio, FileText, Image as ImageIcon, Download, CheckCircle, ArrowLeft, Shield, Cpu, Layers, ChevronDown } from 'lucide-react';

export default function App() {
  const allowedImageTypes = ['image/png', 'image/jpeg', 'image/jpg'];
  const allowedImageExtensions = ['.png', '.jpg', '.jpeg'];
  const maxSecretFileSizeBytes = 2 * 1024 * 1024;
  const capacityErrorMessage = 'The selected image is too small to store this data. Please upload a larger image.';

  const [mode, setMode] = useState('encrypt');
  const [coverImage, setCoverImage] = useState(null);
  const [secretType, setSecretType] = useState('text'); 
  const [secretText, setSecretText] = useState("");
  const [secretFile, setSecretFile] = useState(null);
  
  const [status, setStatus] = useState("idle"); 
  const [resultUrl, setResultUrl] = useState(null);
  const [resultText, setResultText] = useState(null);
  const [extractedName, setExtractedName] = useState("");
  const [errorMessage, setErrorMessage] = useState("");

  const fileToBase64 = (file) => {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.readAsDataURL(file);
      reader.onload = () => resolve(reader.result);
      reader.onerror = reject;
    });
  };

  const getImageCapacityBits = (file) => {
    return new Promise((resolve, reject) => {
      const objectUrl = window.URL.createObjectURL(file);
      const img = new Image();

      img.onload = () => {
        const width = img.naturalWidth || img.width;
        const height = img.naturalHeight || img.height;
        window.URL.revokeObjectURL(objectUrl);
        // Conservative estimate: RGB channels with 1 bit hidden per channel.
        resolve(width * height * 3);
      };

      img.onerror = () => {
        window.URL.revokeObjectURL(objectUrl);
        reject(new Error('Failed to read selected image dimensions.'));
      };

      img.src = objectUrl;
    });
  };

  const isSupportedImageFile = (file) => {
    if (!file) return false;

    const fileName = file.name.toLowerCase();
    const hasValidExtension = allowedImageExtensions.some((ext) => fileName.endsWith(ext));
    const hasValidMimeType = allowedImageTypes.includes(file.type.toLowerCase());

    return hasValidExtension || hasValidMimeType;
  };

  const handleCoverImageChange = (event) => {
    const file = event.target.files?.[0];

    if (!file) {
      return;
    }

    if (!isSupportedImageFile(file)) {
      setCoverImage(null);
      setStatus('error');
      setErrorMessage('Only image files are supported for steganography');
      event.target.value = '';
      return;
    }

    setCoverImage(file);
    if (status === 'error') {
      setStatus('idle');
      setErrorMessage('');
    }
  };

  const handleSubmit = async () => {
    if (!coverImage) {
      setStatus('error');
      setErrorMessage('Please select a valid PNG, JPG, or JPEG image.');
      return;
    }

    if (!isSupportedImageFile(coverImage)) {
      setStatus('error');
      setErrorMessage('Only image files are supported for steganography');
      return;
    }
    
    setStatus("processing");
    setErrorMessage("");
    const formData = new FormData();
    formData.append("image", coverImage);
    const imageCapacityBits = await getImageCapacityBits(coverImage);
    let secretPayload = '';
    const textEncoder = new TextEncoder();
    
    if (mode === 'encrypt') {
      if (secretType === 'text') {
        secretPayload = secretText;
        const requiredBits = textEncoder.encode(secretPayload).length * 8 + 32;
        if (requiredBits > imageCapacityBits) {
          setStatus('error');
          setErrorMessage(capacityErrorMessage);
          return;
        }
        formData.append("message", new Blob([secretPayload], { type: 'text/plain' }), "secret.txt");
      } else {
        if (!secretFile) {
          setStatus('error');
          setErrorMessage('Please select a file to hide.');
          return;
        }

        if (secretFile.size > maxSecretFileSizeBytes) {
          setStatus('error');
          setErrorMessage('Only text or small files are supported.');
          return;
        }

        // Convert file to base64 with a metadata header so it round-trips safely
        const dataUrl = await fileToBase64(secretFile);
        secretPayload = `STEGO_FILE|||${secretFile.name}|||${dataUrl}`;
        const requiredBits = textEncoder.encode(secretPayload).length * 8 + 32;
        if (requiredBits > imageCapacityBits) {
          setStatus('error');
          setErrorMessage(capacityErrorMessage);
          return;
        }
        formData.append("message", new Blob([secretPayload], { type: 'text/plain' }), "secret.txt");
      }
    }

    const endpoint = mode === 'encrypt' 
      ? "http://localhost:8080/encode" 
      : "http://localhost:8080/decode";

    try {
      const config = {
        validateStatus: (statusCode) => statusCode < 500,
      };

      if (mode === 'encrypt') {
        config.responseType = 'blob';
      }
      
      const response = await axios.post(endpoint, formData, config);

      if (response.status >= 400) {
        let errMsg = 'Request failed. Please try another image.';
        if (response.data instanceof Blob) {
          errMsg = await response.data.text();
        } else if (typeof response.data === 'string') {
          errMsg = response.data;
        }
        setErrorMessage(errMsg);
        setStatus('error');
        return;
      }
      
      if (mode === 'encrypt') {
        const url = window.URL.createObjectURL(new Blob([response.data]));
        setResultUrl(url); 
      } else {
        const data = response.data;
        // Detect if hidden data was a file (encoded with our metadata header)
        if (typeof data === 'string' && data.startsWith('STEGO_FILE|||')) {
          const sep = '|||';
          const first = data.indexOf(sep);
          const second = data.indexOf(sep, first + sep.length);
          const filename = data.substring(first + sep.length, second);
          const dataUrl = data.substring(second + sep.length);
          // Decode the base64 data URL into a downloadable blob
          const [header, b64] = dataUrl.split(',');
          const mimeMatch = header.match(/data:([^;]+)/);
          const mimeType = mimeMatch ? mimeMatch[1] : 'application/octet-stream';
          const byteChars = atob(b64);
          const byteArray = new Uint8Array(byteChars.length);
          for (let i = 0; i < byteChars.length; i++) byteArray[i] = byteChars.charCodeAt(i);
          const blob = new Blob([byteArray], { type: mimeType });
          const url = window.URL.createObjectURL(blob);
          setResultUrl(url);
          setExtractedName(filename);
        } else {
          setResultText(data);
          setExtractedName("Secret Message");
        }
      }
      setStatus("success");
    } catch (error) {
      console.error("Error:", error);
      let errMsg = 'Backend Connection Failed. Is the C++ Server running on port 8080?';
      if (error.response?.data) {
        if (error.response.data instanceof Blob) {
          errMsg = await error.response.data.text();
        } else if (typeof error.response.data === 'string') {
          errMsg = error.response.data;
        }
      }
      setErrorMessage(errMsg);
      setStatus("error");
    }
  };
  
  const reset = () => {
    setStatus('idle'); setResultUrl(null); setResultText(null); 
    setSecretText(""); setSecretFile(null); setCoverImage(null);
    setErrorMessage("");
  };

  const scrollToTool = () => {
    document.getElementById('app-tool').scrollIntoView({ behavior: 'smooth' });
  };

  return (
    <div className="min-h-screen bg-[#0a0f1c] text-slate-100 font-sans selection:bg-blue-500/30 overflow-x-hidden">
      
      {/* 1. NAVBAR */}
      <nav className="fixed top-0 w-full z-50 border-b border-white/5 bg-[#0a0f1c]/80 backdrop-blur-md">
        <div className="max-w-7xl mx-auto px-6 h-20 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="bg-gradient-to-tr from-blue-600 to-purple-600 p-2 rounded-xl">
              <Shield className="w-6 h-6 text-white" />
            </div>
            <span className="text-xl font-bold tracking-widest text-transparent bg-clip-text bg-gradient-to-r from-white to-slate-400">
              STEGO-VAULT
            </span>
          </div>
          <button onClick={scrollToTool} className="text-sm font-semibold text-slate-300 hover:text-white transition-colors">
            Launch App
          </button>
        </div>
      </nav>

      {/* 2. HERO SECTION */}
      <section className="pt-40 pb-20 px-6 relative flex flex-col items-center text-center min-h-[80vh] justify-center">
        {/* Background glow effects */}
        <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[600px] h-[600px] bg-blue-600/20 blur-[120px] rounded-full pointer-events-none" />
        <div className="absolute top-1/2 left-1/4 w-[400px] h-[400px] bg-purple-600/20 blur-[100px] rounded-full pointer-events-none" />

        <motion.div initial={{ opacity: 0, y: 30 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.8 }}>
          <h1 className="text-5xl md:text-7xl font-extrabold tracking-tight mb-6">
            Hide Secrets in <br className="hidden md:block" />
            <span className="text-transparent bg-clip-text bg-gradient-to-r from-blue-400 via-indigo-400 to-purple-400">
              Plain Sight.
            </span>
          </h1>
          <p className="text-lg md:text-xl text-slate-400 max-w-2xl mx-auto mb-10 leading-relaxed">
            Military-grade steganography powered by Huffman Coding and Spiral Matrix Traversal. Securely embed text, audio, and images inside standard cover photos.
          </p>
          
          <button onClick={scrollToTool} className="group relative inline-flex items-center justify-center gap-3 px-8 py-4 bg-white text-black font-bold rounded-full overflow-hidden transition-transform hover:scale-105">
            <span>Start Encrypting</span>
            <ChevronDown className="w-5 h-5 group-hover:translate-y-1 transition-transform" />
          </button>
        </motion.div>
      </section>

      {/* 3. THE TOOL SECTION */}
      <section id="app-tool" className="py-24 px-4 relative z-10 flex justify-center bg-black/40 border-y border-white/5">
        <motion.div 
          initial={{ opacity: 0, scale: 0.95 }}
          whileInView={{ opacity: 1, scale: 1 }}
          viewport={{ once: true }}
          transition={{ duration: 0.5 }}
          className="w-full max-w-xl backdrop-blur-2xl bg-white/5 border border-white/10 rounded-3xl shadow-[0_0_50px_rgba(0,0,0,0.3)] overflow-hidden"
        >
          {/* Header Tabs */}
          <div className="flex border-b border-white/10">
            <button onClick={() => { setMode('encrypt'); reset(); }} className={`flex-1 py-5 text-sm font-bold tracking-widest uppercase transition-all flex items-center justify-center gap-2 ${mode === 'encrypt' ? 'bg-blue-500/20 text-blue-400 border-b-2 border-blue-400' : 'text-slate-500 hover:bg-white/5'}`}>
              <Lock className="w-4 h-4" /> Hide Data
            </button>
            <button onClick={() => { setMode('decrypt'); reset(); }} className={`flex-1 py-5 text-sm font-bold tracking-widest uppercase transition-all flex items-center justify-center gap-2 ${mode === 'decrypt' ? 'bg-purple-500/20 text-purple-400 border-b-2 border-purple-400' : 'text-slate-500 hover:bg-white/5'}`}>
              <Unlock className="w-4 h-4" /> Extract Data
            </button>
          </div>

          <div className="p-8">
            <AnimatePresence mode="wait">
              {status === 'success' ? (
                <motion.div key="success" initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -20 }} className="text-center space-y-6">
                  <CheckCircle className={`w-20 h-20 mx-auto ${mode === 'encrypt' ? 'text-blue-400' : 'text-purple-400'} drop-shadow-[0_0_15px_rgba(59,130,246,0.5)]`} />
                  <h2 className="text-2xl font-light text-white">{mode === 'encrypt' ? 'Data Hidden Successfully' : 'Secret Extracted!'}</h2>
                  
                  {mode === 'encrypt' ? (
                    resultUrl ? (
                      <a href={resultUrl} download="stego_locked.png" className="flex items-center justify-center gap-2 w-full bg-gradient-to-r from-blue-600 to-cyan-600 hover:from-blue-500 hover:to-cyan-500 text-white font-bold py-4 rounded-xl transition-all shadow-lg">
                        <Download className="w-5 h-5" /> Download Locked Image
                      </a>
                    ) : (
                      <div className="text-slate-400 text-sm bg-white/5 p-4 rounded-xl border border-white/10">
                        Image saved successfully to the backend server folder as <strong>temp_encoded.png</strong>.
                      </div>
                    )
                  ) : (
                    <div className="bg-black/40 p-6 rounded-2xl border border-white/5 text-left">
                      <p className="text-xs text-slate-500 uppercase tracking-wider mb-3">Extracted Data: <span className="text-purple-400 font-bold">{extractedName}</span></p>
                      {resultText ? (
                        <p className="text-white font-mono break-all leading-relaxed">{resultText}</p>
                      ) : (
                        <a href={resultUrl} download={extractedName} className="flex items-center justify-center gap-2 w-full bg-gradient-to-r from-purple-600 to-pink-600 hover:from-purple-500 hover:to-pink-500 text-white font-bold py-4 rounded-xl transition-all shadow-lg mt-4">
                          <Download className="w-5 h-5" /> Download {extractedName}
                        </a>
                      )}
                    </div>
                  )}

                  <button onClick={reset} className="flex items-center justify-center gap-2 text-sm text-slate-400 hover:text-white mx-auto mt-4 transition-colors">
                    <ArrowLeft className="w-4 h-4" /> Process Another
                  </button>
                </motion.div>
              ) : (
                <motion.div key="input" initial={{ opacity: 0, x: -20 }} animate={{ opacity: 1, x: 0 }} exit={{ opacity: 0, x: 20 }} className="space-y-6">
                  
                  {/* Cover Image Upload */}
                  <div>
                    <label className="block text-xs uppercase tracking-widest text-slate-400 mb-2">1. Cover Image (PNG/JPG)</label>
                    <div className={`relative border-2 border-dashed rounded-2xl p-8 text-center transition-all ${coverImage ? 'border-blue-500/50 bg-blue-500/10' : 'border-white/10 hover:border-white/30 hover:bg-white/5'}`}>
                      <input type="file" accept=".png,.jpg,.jpeg,image/png,image/jpeg" onChange={handleCoverImageChange} className="absolute inset-0 w-full h-full opacity-0 cursor-pointer" />
                      {coverImage ? (
                        <span className="text-blue-300 font-medium">{coverImage.name}</span>
                      ) : (
                        <div className="flex flex-col items-center text-slate-500">
                          <Upload className="w-8 h-8 mb-3 opacity-50" />
                          <span>Click or Drag Image Here</span>
                        </div>
                      )}
                    </div>
                  </div>

                  {/* Secret Data Input */}
                  {mode === 'encrypt' && (
                    <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }}>
                      <div className="flex items-center justify-between mb-2">
                        <label className="text-xs uppercase tracking-widest text-slate-400">2. Secret Data to Hide</label>
                        <div className="flex gap-2 bg-black/40 p-1 rounded-lg">
                          <button onClick={() => setSecretType('text')} className={`px-3 py-1 rounded text-xs transition-all ${secretType === 'text' ? 'bg-slate-700 text-white' : 'text-slate-400'}`}>Text</button>
                          <button onClick={() => setSecretType('file')} className={`px-3 py-1 rounded text-xs transition-all ${secretType === 'file' ? 'bg-slate-700 text-white' : 'text-slate-400'}`}>File</button>
                        </div>
                      </div>

                      {secretType === 'text' ? (
                        <textarea value={secretText} onChange={(e) => setSecretText(e.target.value)} placeholder="Type your secret message..." className="w-full bg-black/20 border border-white/10 rounded-2xl p-4 focus:outline-none focus:border-blue-500/50 transition-all h-32 resize-none text-slate-200" />
                      ) : (
                        <div className={`relative border-2 border-dashed rounded-2xl p-6 text-center transition-all ${secretFile ? 'border-green-500/50 bg-green-500/10' : 'border-white/10 hover:border-white/30 hover:bg-white/5'}`}>
                          <input type="file" onChange={(e) => setSecretFile(e.target.files[0])} className="absolute inset-0 w-full h-full opacity-0 cursor-pointer" />
                          {secretFile ? (
                            <span className="text-green-300 font-medium">{secretFile.name}</span>
                          ) : (
                            <div className="flex items-center justify-center gap-4 text-slate-500">
                              <FileAudio className="w-6 h-6 opacity-50" />
                              <ImageIcon className="w-6 h-6 opacity-50" />
                              <FileText className="w-6 h-6 opacity-50" />
                            </div>
                          )}
                        </div>
                      )}
                    </motion.div>
                  )}

                  <button onClick={handleSubmit} disabled={status === 'processing'} className={`w-full font-bold py-4 rounded-xl transition-all shadow-lg ${mode === 'encrypt' ? 'bg-gradient-to-r from-blue-600 to-cyan-600 hover:from-blue-500 hover:to-cyan-500 text-white' : 'bg-gradient-to-r from-purple-600 to-pink-600 hover:from-purple-500 hover:to-pink-500 text-white'} disabled:opacity-50`}>
                    {status === 'processing' ? 'Processing Matrix...' : (mode === 'encrypt' ? 'Lock Data Inside Image' : 'Extract Hidden Data')}
                  </button>
                  
                  {status === 'error' && <div className="text-red-400 text-center text-sm mt-4 bg-red-500/10 p-3 rounded-lg border border-red-500/20">{errorMessage || 'Backend Connection Failed. Is the C++ Server running on port 8080?'}</div>}
                </motion.div>
              )}
            </AnimatePresence>
          </div>
        </motion.div>
      </section>

      {/* 4. FEATURES / DSA SECTION */}
      <section className="py-24 px-6 max-w-7xl mx-auto">
        <div className="text-center mb-16">
          <h2 className="text-3xl font-bold mb-4">Core Architecture</h2>
          <p className="text-slate-400 max-w-2xl mx-auto">Engineered from scratch using C++ Object-Oriented principles and advanced Data Structures for maximum security.</p>
        </div>
        
        <div className="grid md:grid-cols-3 gap-8">
          <div className="bg-white/5 border border-white/10 p-8 rounded-3xl hover:bg-white/10 transition-colors">
            <Cpu className="w-10 h-10 text-blue-400 mb-6" />
            <h3 className="text-xl font-bold mb-3">Greedy Compression</h3>
            <p className="text-slate-400 leading-relaxed">Utilizes <strong className="text-slate-200">Huffman Coding</strong> with a Min-Heap Priority Queue to compress payloads, maximizing the storage capacity of the cover image.</p>
          </div>
          <div className="bg-white/5 border border-white/10 p-8 rounded-3xl hover:bg-white/10 transition-colors">
            <Layers className="w-10 h-10 text-purple-400 mb-6" />
            <h3 className="text-xl font-bold mb-3">OOP Architecture</h3>
            <p className="text-slate-400 leading-relaxed">Built with <strong className="text-slate-200">Encapsulation and RAII</strong> in C++ to ensure modular, memory-safe, and highly readable system design.</p>
          </div>
          <div className="bg-white/5 border border-white/10 p-8 rounded-3xl hover:bg-white/10 transition-colors">
            <Shield className="w-10 h-10 text-cyan-400 mb-6" />
            <h3 className="text-xl font-bold mb-3">Bitwise Steganography</h3>
            <p className="text-slate-400 leading-relaxed">Manipulates the <strong className="text-slate-200">Least Significant Bit (LSB)</strong> of pixel data through low-level C++ bitwise operations, keeping changes invisible to the human eye.</p>
          </div>
        </div>
      </section>

      {/* 5. FOOTER */}
      <footer className="py-8 border-t border-white/5 text-center text-slate-500">
        <p className="text-sm">
          Built by <span className="text-slate-300 font-semibold">Group 11</span> | Project © 2026
        </p>
      </footer>

    </div>
  );
}