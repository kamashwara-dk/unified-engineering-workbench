import React, { ReactNode } from 'react';
import { motion, useReducedMotion } from 'framer-motion';

interface Props {
  viewKey: string;
  children: ReactNode;
}

export const ViewTransition: React.FC<Props> = ({ viewKey, children }) => {
  const shouldReduceMotion = useReducedMotion();

  if (shouldReduceMotion) {
    return <div className="w-full h-full flex flex-col flex-1">{children}</div>;
  }

  return (
    <motion.div
      key={viewKey}
      initial={{ opacity: 0, y: 4 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0, y: -4 }}
      transition={{ duration: 0.15, ease: 'easeOut' }}
      className="w-full h-full flex flex-col flex-1"
    >
      {children}
    </motion.div>
  );
};

